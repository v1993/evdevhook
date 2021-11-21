/*
    Evdevhook - DSU server for motion from evdev compatible joysticks
    Copyright (C) 2020  Valeri Ochinski

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdint>
#include <cstring>
#include <iostream>
#include <numeric>

#include <unistd.h>
#include <fcntl.h>

#include "VirtualDevice.hpp"
#include "globals.hpp"

VirtualDevice::VirtualDevice(uint8_t number_): number(number_) {};

VirtualDevice::~VirtualDevice() {
	Disconnect();
}

bool VirtualDevice::Connect(libevdev* device) noexcept {
	Disconnect(); // Just in case
	dev = device;

	// Make sure that we have at least accelerometer
	for (int code = ABS_X; code < ABS_Z + 1; ++code) {
		if (!libevdev_has_event_code(dev, EV_ABS, code)) {
			std::cout << "Accelerometer not found, device won't work\n";
			return false;
		}
	}

	// We can work without gyro, but it's a little sad that way
	have_gyro = true;
	for (int code = ABS_RX; code < ABS_RZ + 1; ++code) {
		have_gyro &= libevdev_has_event_code(dev, EV_ABS, code);
	}

	if (!have_gyro) {
		std::cout << "Gyro not found, only limited functional will be available\n";
	}

	// Read information for each axis
	for (uint8_t i = ABS_X; i <= (have_gyro ? ABS_RZ : ABS_Z); ++i) {
		auto* const info = libevdev_get_abs_info(dev, i);
		center[i] = std::midpoint(info->minimum, info->maximum);
		resolution[i] = info->resolution;
	};

	timestamp = 0;

	// Add a profile option to enfoce this fallack?
	have_timestamp_event = libevdev_has_event_code(dev, EV_MSC, MSC_TIMESTAMP);

	if (!have_timestamp_event) {
		std::cout << "Accurate timestamping of motion unavailable, using fallback\n";
	}

	source = Glib::IOSource::create(libevdev_get_fd(dev), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
	source->connect(sigc::mem_fun(*this, &VirtualDevice::onInput));
	source->attach(g_mainloop->get_context());

	return true;
}

void VirtualDevice::Disconnect() {
	if (dev) {
		auto fd = libevdev_get_fd(dev);
		libevdev_free(dev);
		source.reset();
		close(fd);
		dev = nullptr;
	}
}

bool VirtualDevice::onInput(Glib::IOCondition condition) {
	if (condition & Glib::IOCondition::IO_HUP) {
		// Device was disconnected from computer
		// We don't actually need udev for this, hooray!
		Disconnect();
		std::cout << conf.name << " was disconnected" << '\n';
		return false;
	}

	if (condition & Glib::IOCondition::IO_IN) {
		struct input_event ev;
		// FIXME: allow updating data only on accelerometer changes?

		int rc;
		do {
			rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

			if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
				switch (ev.type) {
				case EV_SYN: {
					processSync(ev.time);
				}
				break;
				case EV_MSC:
					if (ev.code == MSC_TIMESTAMP) {
						// Note: if device lacks this event code (check have_timestamp_event), fallback in processSync is used
						updateTimestamp(ev.value);
					}
					break;
				case EV_ABS:
					updateAxis(ev.code, ev.value);
					break;
				}
			}
		} while (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC);
	};

	return true;
}

void VirtualDevice::processSync(struct timeval& time) {
	if (clients.size() == 0) return; // Nobody is listening, good

	// Fallback for drivers lacking fully accurate motion timing
	if (!have_timestamp_event) {
		timestamp = uint64_t(time.tv_sec) * 1000000 + uint64_t(time.tv_usec);
	}

	// Five seconds timeout
	gint64 timeoutBefore = g_get_monotonic_time() - 5000000;

	// Setup common elements for array (maybe even allocate it only once per device not per call?)
	std::array<char, 100> arr;
	arr.fill(0);

	constexpr size_t headerOffset = 20;
	FillSlotHeader(reinterpret_cast<ControllerSlotHeader*>(&arr[headerOffset]));
	arr[headerOffset + 11] = 1; // Is connected
	std::memset(&arr[headerOffset + 20], 127, 4); // Sticks at their centers
	*reinterpret_cast<uint64_t*>(&arr[headerOffset + 48]) = timestamp; // Motion timestamp
	std::memcpy(&arr[headerOffset + 56], state.data(), state.size()*sizeof(float)); // Motion data

	for (auto it = clients.begin(); it != clients.end();) {
		auto clientId = it->first;
		if (it->second.requestTime < timeoutBefore) {
			it = clients.erase(it);
			PacketCounter::GetInstance().RemoveRequester(clientId);
		} else {
			*reinterpret_cast<uint32_t*>(&arr[headerOffset + 12]) = PacketCounter::GetInstance().NewPacketNum(clientId);
			AddHeaderAndSend({arr.data(), arr.size()}, 0x100002, it->second.addr);

			++it;
		}
	}
}


void VirtualDevice::updateAxis(uint16_t axis, int32_t value) {
	if (axis <= ABS_RZ) {
		auto const idx = conf.profile.mapping[axis];
		if (idx != -1) {
			int64_t valueCentered = static_cast<int64_t>(value) - static_cast<int64_t>(center[axis]);

			if (conf.profile.invert[axis])
				valueCentered *= -1;

			state[idx] = static_cast<double>(valueCentered) / resolution[axis];
			if (axis >= ABS_RX)
				state[idx] *= conf.profile.gyroSensitivity;
		}
	}
}

void VirtualDevice::updateTimestamp(int32_t eventTimestamp) {
	// It wraps around each 1.2 hours (14 half-lifes of uranium-241, apparently), so we need to handle this
	constexpr uint64_t signBit = 1ull << 31;
	const int32_t shortOldValue = static_cast<int32_t>(timestamp & ~signBit); // How it looks without higher bits
	if (shortOldValue > eventTimestamp) {
		// Wrapping have happened, account for it
		timestamp += signBit;
	}

	if (timestamp < signBit) {
		// It will go this way for slighty more than an hour of gameplay
		timestamp = eventTimestamp;
	} else {
		// That's what you get for playing for too long
		// Cuts off everything after sign bit and overwrites it with new timestamp. Should be optimal in terms of speed.
		timestamp = (timestamp & ~(signBit - 1)) | eventTimestamp;
	}
}

void VirtualDevice::FillSlotHeader(ControllerSlotHeader* info) {
	info->slotnum = number;
	info->connectionStatus = (dev ? 2 : 0);
	if (dev) {
		info->model = (have_gyro ? 2 : 1);
		info->connectionType = 0;
		info->mac = name_hash; // Hash of name is the best we can get for uniqueness, I guess
		info->battery = 0;
	}
}

void VirtualDevice::ReportRequest(uint32_t id, Glib::RefPtr<Gio::SocketAddress> addr) {
	auto it = clients.find(id);

	if (it == clients.end()) {
		clients.emplace(id, ClientDescription {.addr = addr, .requestTime = g_get_monotonic_time()});
		PacketCounter::GetInstance().AddRequester(id);
	} else {
		// Update timeout
		it->second.requestTime = g_get_monotonic_time();
	}
}

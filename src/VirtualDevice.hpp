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

#pragma once

#include <libevdev/libevdev.h>

#include <glibmm/main.h>
#include <giomm.h>

#include <cstdint>
#include <array>
#include <bitset>
#include <unordered_map>

#include "packet.hpp"

// We generally assume this
static_assert((ABS_X == 0) && (ABS_Z == 2) && (ABS_RX == 3) && (ABS_RZ == 5), "weird axis constants");
// Virtual axles also generally conform to this order

/// Indexes stand for evdev codes
struct OrientationProfile {
	std::array<std::int8_t, 6> mapping {-1, -1, -1, -1, -1, -1}; ///< Which virtual axis is activated by given input
	std::bitset<6> invert {false}; ///< Should it be inverted

	double gyroSensitivity = 1.0; ///< Multiplier for gyro values
};

struct DeviceConfiguration {
	std::string name;
	OrientationProfile profile;

	// std::array<std::int32_t, 6> calibration {0}; ///< TODO: Raw calibration value to apply
};

struct ClientDescription {
	Glib::RefPtr<Gio::SocketAddress> addr;
	gint64 requestTime; // Monotonic time from g_get_monotonic_time() for timeouts
};

class VirtualDevice {
	public:
		VirtualDevice() = delete;
		VirtualDevice(uint8_t number_);
		VirtualDevice(const VirtualDevice&) = delete;
		VirtualDevice(VirtualDevice&&) = delete;
		~VirtualDevice();

		void SetConfig(DeviceConfiguration&& conf_) {
			conf = conf_;
			name_hash = std::hash<std::string>()(conf.name);
		};

		// On false, call "Disconnect"
		bool Connect(libevdev* device) noexcept;
		void Disconnect();
		bool IsConnected() { return dev; };
		size_t GetMac() { return name_hash; };

		void FillSlotHeader(ControllerSlotHeader* info);

		void ReportRequest(uint32_t id, Glib::RefPtr<Gio::SocketAddress> addr);
	private:
		bool onInput(Glib::IOCondition);

		void processSync(struct timeval& ev);
		void updateTimestamp(int32_t eventTimestamp);
		void updateAxis(uint16_t axis, int32_t value);

		DeviceConfiguration conf;
		size_t name_hash: 48;
		const uint8_t number;
		libevdev* dev = nullptr;

		std::array<float, 6> state;

		// Kernel only reports 32-bit timestamp, so we try to compensate for this
		uint64_t timestamp = 0;

		std::array<std::int32_t, 6> center;
		std::array<double, 6> resolution;

		bool have_gyro;
		bool have_timestamp_event;

		Glib::RefPtr<Glib::IOSource> source;

		std::unordered_map<uint32_t, ClientDescription> clients;
};

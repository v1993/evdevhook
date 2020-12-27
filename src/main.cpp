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

#include <iostream>
#include <fstream>
#include <random>

#include <giomm.h>

#include <libudev.h>
#include <libevdev/libevdev.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib-unix.h>

#include <nlohmann/json.hpp>

#include "globals.hpp"
#include "packet.hpp"

Glib::RefPtr<Gio::Socket> g_socket; ///< Global socket to use
Glib::RefPtr<Glib::MainLoop> g_mainloop; ///< Main loop used by application

// Assign a number to each device
std::array<VirtualDevice, SLOT_COUNT> g_devices {0, 1, 2, 3};

std::unordered_map<std::string, std::uint8_t> g_name_to_devidx;

// Fits our needs just fine
uint32_t g_server_id{std::random_device()()};
uint8_t g_devcount {};

namespace {
	struct udevHandle {
		public:
			udevHandle(): handle(udev_new()) {
				if (!handle)
					throw std::runtime_error("Failed to obtain udev object");
			};
			~udevHandle() {
				if (handle) {
					udev_unref(handle);
				}
			};
			udevHandle(const udevHandle&) = delete;

			udev* getHandle() const { return handle; };

		protected:
			udev* const handle;
	};

	/// Create profile from json description
	/// Input must be valid json object!
	OrientationProfile ParseProfile(auto& j) {
		OrientationProfile prof;

		auto parseData = [&prof](auto & desc, int firstIdx) {
			if (desc.is_null()) {
				return false;
			}
			if (!desc.is_string()) {
				throw std::logic_error("orientation description isn't string");
			}

			std::string str = desc;
			if (str.length() != 6) {
				throw std::logic_error("orientation description isn't 6 characters long");
			}

			for (uint8_t i = 0; i < 3; ++i) {
				uint8_t evdevAxis = firstIdx;
				bool invert;

				switch (str[2 * i]) {
				case 'x':
				case 'X':
					break;
				case 'y':
				case 'Y':
					evdevAxis += 1;
					break;
				case 'z':
				case 'Z':
					evdevAxis += 2;
					break;
				default:
					throw std::logic_error("incorrect orientation axis specifier letter");
				}

				switch (str[2 * i + 1]) {
				case '+':
					invert = false;
					break;
				case '-':
					invert = true;
					break;
				default:
					throw std::logic_error("incorrect orientation axis specifier sign");
				}

				if (prof.mapping[evdevAxis] != -1)
					throw std::logic_error("can't assign same physical axis to two virtual ones");

				prof.mapping[evdevAxis] = firstIdx + i;
				prof.invert[evdevAxis] = invert;
			}
			return true;
		};

		if (!parseData(j["accel"], ABS_X)) {
			std::cerr << "Warning: missing accelerometer binding. Joystick won't work well if at all.\n";
		}
		if (!parseData(j["gyro"], ABS_RX)) {
			std::cerr << "Warning: missing gyroscope binding. Don't mind if it have no gyrosope.\n";
		}
		return prof;
	};

	void LoadConfig(std::istream& source) {
		using json = nlohmann::json;
		json j;

		if (!(source >> j && j.is_object() && j["devices"].is_array() && j["profiles"].is_object())) {
			throw std::logic_error("failed to parse config file");
		}

		auto& devices = j["devices"];
		auto& profiles = j["profiles"];

		if (devices.size() > 4) {
			throw std::logic_error("too many devices (>4)");
		}

		uint8_t devnum = 0;
		for (auto& dev : devices) {
			if (!(dev.is_object() && dev["name"].is_string() && dev["profile"].is_string())) {
				throw std::logic_error("invalid device record");
			}

			std::string name = dev["name"];
			if (g_name_to_devidx.contains(name)) {
				throw std::logic_error("dublicate device `" + name + "`");
			}
			std::string profileName = dev["profile"];

			auto profileDesc = profiles[profileName];
			if (!profileDesc.is_object()) {
				throw std::logic_error("invalid profile `" + profileName + "`");
			}

			// TODO: pass name for better errors
			auto profile = ParseProfile(profileDesc); // TODO: cache profiles

			// TODO: read calibration data

			g_name_to_devidx.emplace(name, devnum);
			DeviceConfiguration devconf;

			devconf.name = std::move(name);
			devconf.profile = std::move(profile);

			g_devices[devnum].SetConfig(std::move(devconf));

			++devnum;
		}
		g_devcount = devnum;
	}

	void AddDevice(const char* path) {
		const int fd = ::open(path, O_RDWR | O_NONBLOCK);
		if (fd == -1)
			return;

		libevdev* dev = nullptr;
		if (libevdev_new_from_fd(fd, &dev) != 0) {
			close(fd);
			return;
		}

		if (libevdev_has_property(dev, INPUT_PROP_ACCELEROMETER)) {
			std::cout << "Found motion device: " << libevdev_get_name(dev) << "\n";
			auto it = g_name_to_devidx.find(libevdev_get_name(dev));
			if (it != g_name_to_devidx.end()) {
				std::cout << "Connecting...";
				if (g_devices[it->second].Connect(dev)) {
					std::cout << " done!\n";
				} else {
					g_devices[it->second].Disconnect();
					std::cout << " failed!\n";
				}

			}
		}
	};

	int OnSigint(void*) {
		g_mainloop->quit();
		return false;
	};
}

int main(int argc, char* argv[]) {
	Gio::init();
	g_mainloop = Glib::MainLoop::create();
	std::cout << std::boolalpha;

	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " config_file" << std::endl;
		std::exit(2);
	}

	// Parse config file here
	{
		std::ifstream config{argv[1]};
		LoadConfig(config);
	}

	udevHandle udev{};

	// Enumerate connected devices
	// Heavily based on Dolphin's code
	{
		udev_enumerate* const enumerate = udev_enumerate_new(udev.getHandle());
		udev_enumerate_add_match_subsystem(enumerate, "input");
		udev_enumerate_scan_devices(enumerate);
		udev_list_entry* const devices = udev_enumerate_get_list_entry(enumerate);

		udev_list_entry* dev_list_entry;
		udev_list_entry_foreach(dev_list_entry, devices) {
			const char* path = udev_list_entry_get_name(dev_list_entry);
			udev_device* dev = udev_device_new_from_syspath(udev.getHandle(), path);

			if (const char* devnode = udev_device_get_devnode(dev))
				AddDevice(devnode);

			udev_device_unref(dev);
		};
		udev_enumerate_unref(enumerate);
	}

	// Hotplug monitor

	auto monitor = std::shared_ptr<udev_monitor> {udev_monitor_new_from_netlink(udev.getHandle(), "udev"), udev_monitor_unref};
	udev_monitor_filter_add_match_subsystem_devtype(monitor.get(), "input", nullptr);
	udev_monitor_enable_receiving(monitor.get());

	auto monitor_source = Glib::IOSource::create(udev_monitor_get_fd(monitor.get()), Glib::IOCondition::IO_IN);
	monitor_source->connect([&monitor](Glib::IOCondition) {
		while (auto dev = std::shared_ptr<udev_device>(udev_monitor_receive_device(monitor.get()), udev_device_unref)) {
			const char* const devnode = udev_device_get_devnode(dev.get());
			if (!devnode)
				continue;
			if (strcmp(udev_device_get_action(dev.get()), "add") == 0) {
				AddDevice(devnode);
			}
		}
		return true;
	});
	monitor_source->attach(g_mainloop->get_context());

	// Setup socket
	g_socket = Gio::Socket::create(Gio::SocketFamily::SOCKET_FAMILY_IPV4, Gio::SocketType::SOCKET_TYPE_DATAGRAM, Gio::SocketProtocol::SOCKET_PROTOCOL_UDP);
	g_socket->set_blocking(true); // Right?..
	g_socket->bind(Gio::InetSocketAddress::create(Gio::InetAddress::create_loopback(Gio::SocketFamily::SOCKET_FAMILY_IPV4), 26760), false);
	auto socket_source = g_socket->create_source(Glib::IOCondition::IO_IN);
	std::array<char, 256> buf;
	socket_source->connect([&buf](Glib::IOCondition) {
		Glib::RefPtr<Gio::SocketAddress> addr;
		size_t size = g_socket->receive_from(addr, buf.data(), buf.size());
		ProcessIncoming(addr, {buf.data(), size});
		return true;
	});
	socket_source->attach(g_mainloop->get_context());

	// I'd very much prefer C++ version, but there doesn't seem to be one?..
	g_unix_signal_add(SIGINT, OnSigint, nullptr);
	g_mainloop->run();
	std::cout << "Exiting" << std::endl;
};

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

#include <array>
#include <unordered_map>

#include <glibmm/main.h>
#include <giomm/socket.h>

#include "VirtualDevice.hpp"
#include "constants.hpp"

extern Glib::RefPtr<Gio::Socket> g_socket; ///< Global socket to use
extern Glib::RefPtr<Glib::MainLoop> g_mainloop; ///< Main loop used by application

// Assign a number to each device
extern std::array<VirtualDevice, SLOT_COUNT> g_devices;

extern std::unordered_map<std::string, std::uint8_t> g_name_to_devidx;

extern uint32_t g_server_id;
extern uint8_t g_devcount;

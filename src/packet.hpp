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

#include <giomm/socketaddress.h>

#include <string_view>
#include <unordered_map>

struct ControllerSlotHeader {
	uint8_t slotnum;
	uint8_t connectionStatus;
	uint8_t model;
	uint8_t connectionType;
	uint64_t mac: 48;
	uint8_t battery;
} __attribute__((packed));
static_assert(sizeof(ControllerSlotHeader) == 11, "ControllerSlotHeader not packed");

class PacketCounter {
	protected:
		struct ClientInfo {
			uint32_t RefCount;
			uint32_t PacketNum;
		};

		std::unordered_map<uint32_t, ClientInfo> map;
	public:
		void AddRequester(uint32_t id);
		void RemoveRequester(uint32_t id);
		uint32_t NewPacketNum(uint32_t id);

		static PacketCounter& GetInstance() {
			static PacketCounter instance;
			return instance;
		};
	private:
		PacketCounter() = default;
		~PacketCounter() = default;
};

void ProcessIncoming(Glib::RefPtr<Gio::SocketAddress> addr, std::string_view p);
void AddHeaderAndSend(std::string_view p, uint32_t messageType, Glib::RefPtr<Gio::SocketAddress> addr);

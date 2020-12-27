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

#include <cstring>

#include <zlib.h>

#include "packet.hpp"
#include "VirtualDevice.hpp"
#include "globals.hpp"

void PacketCounter::AddRequester(uint32_t id) {
	auto it = map.find(id);
	if (it == map.end()) {
		map.emplace(id, ClientInfo {.RefCount = 1, .PacketNum = 0});
	} else {
		++it->second.RefCount;
	}
}

void PacketCounter::RemoveRequester(uint32_t id) {
	auto it = map.find(id);
	if (it != map.end()) {
		if (--it->second.RefCount == 0) {
			map.erase(it);
		}
	} else {
		throw std::logic_error("trying to remove nonexistent client");
	}
}

uint32_t PacketCounter::NewPacketNum(uint32_t id) {
	auto it = map.find(id);
	if (it != map.end()) {
		return it->second.PacketNum++;
	} else {
		throw std::logic_error("trying to send to nonexistent client");
	}
}


namespace {
	struct PacketHeader {
		std::array<char, 4> magic;
		uint16_t version;
		uint16_t length;
		uint32_t CRC32;
		uint32_t id;
	} __attribute__((packed));
	static_assert(sizeof(PacketHeader) == 16, "PacketHeader not packed");

	struct RequestHeader {
		uint8_t actions;
		uint8_t slot;
		uint64_t mac: 48;
	} __attribute__((packed));
	static_assert(sizeof(RequestHeader) == 8, "PacketHeader not packed");

	uint32_t CalculateCrc32(std::string_view str) {
		return crc32(0L, reinterpret_cast<const unsigned char*>(str.data()), str.size());
	};

	void FillHeaderIn(std::string_view p, uint32_t messageType) {
		*(const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&p[16]))) = messageType;
		auto header = const_cast<PacketHeader*>(reinterpret_cast<const PacketHeader*>(p.data()));
		header->magic = {'D', 'S', 'U', 'S'};
		header->version = 1001;
		header->length = p.size() - 16;
		header->CRC32 = 0L;
		header->id = g_server_id;
		header->CRC32 = CalculateCrc32(p);
	}
}

void AddHeaderAndSend(std::string_view p, uint32_t messageType, Glib::RefPtr<Gio::SocketAddress> addr) {
	FillHeaderIn(p, messageType);
	g_socket->send_to(addr, p.data(), p.size());
}


void ProcessIncoming(Glib::RefPtr<Gio::SocketAddress> addr, std::string_view p) {
	using namespace std::literals;
	// Ensure that there's header to parse
	if (p.length() < 16) return;
	// Is it just random crap having nothing to do with us?
	if (p.substr(0, 4) != "DSUC"sv) return;

	auto header = const_cast<PacketHeader*>(reinterpret_cast<const PacketHeader*>(p.data()));
	if (header->version != 1001) return;
	{
		// Length handling
		uint16_t len = header->length + 16;
		if (len < 20) return;
		if (len < p.size()) return;
		if (len > p.size()) p = {p.data(), len};
	}
	{
		// Check CRC32
		const uint32_t crc_expected = header->CRC32;
		header->CRC32 = 0L;
		const uint32_t crc_actual = CalculateCrc32(p);
		if (crc_expected != crc_actual) return;
	}

	// If we got this far, message is probably good
	uint32_t clientId = header->id;

	uint32_t messageType = *(reinterpret_cast<const uint32_t*>(&p[16]));
	
	std::string_view pDat = {&p[20], p.size() - 20};
	switch (messageType) {
	case 0x100000:
		// Protocol version request
	{
		// Header + uint16_t
		std::array < char, 20 + 2 > pOut;
		*reinterpret_cast<uint16_t*>(pOut.data()) = 1001;
		std::string_view outView {pOut.data(), pOut.size()};
		AddHeaderAndSend(outView, messageType, addr);
	}
	break;

	case 0x100001:
		// Info about connected controllers
	{
		if (pDat.size() < (sizeof(int32_t) + 1)) return;
		int slotCnt = std::min(*reinterpret_cast<const int32_t*>(&pDat[0]), static_cast<int32_t>(pDat.size() - sizeof(int32_t)));
		// Header + ControllerSlotHeader + zero byte
		std::array < char, 20 + sizeof(ControllerSlotHeader) + 1 > pOut;
		std::string_view outView {pOut.data(), pOut.size()};
		pOut.fill(0);
		for (int i = 0; i < slotCnt; ++i) {
			if (uint8_t slot = pDat[sizeof(int32_t) + i]; slot < 4) {
				g_devices[slot].FillSlotHeader(reinterpret_cast<ControllerSlotHeader*>(&pOut[20]));
				AddHeaderAndSend(outView, messageType, addr);
			}
		}
	}
	break;

	case 0x100002:
		// Request for controller data
	{
		if (pDat.size() < sizeof(RequestHeader)) return;
		auto req = reinterpret_cast<const RequestHeader*>(pDat.data());

		if (req->actions == 0) {
			for (auto& vdev : g_devices) {
				vdev.ReportRequest(clientId, addr);
			}
			return;
		}

		if ((req->actions & 0x1) && (req->slot < 4)) {
			g_devices[req->slot].ReportRequest(clientId, addr);
		}

		if (req->actions & 0x2) {
			for (auto& vdev : g_devices) {
				if (vdev.GetMac() == req->mac) {
					vdev.ReportRequest(clientId, addr);
					break;
				}
			}
		}
	}
	break;
	};
}

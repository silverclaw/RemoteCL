// This file is part of RemoteCL.

// RemoteCL is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// RemoteCL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with RemoteCL.  If not, see <https://www.gnu.org/licenses/>.

#if !defined(REMOTECL_PACKET_CONTEXT_H)
#define REMOTECL_PACKET_CONTEXT_H
/// @file context.h Wraps packets for cl_context-related functions.

#include <vector>

#include "packets/packet.h"
#include "packets/IDs.h"
#include "socketstream.h"
#include "streamserialise.h"

namespace RemoteCL
{
struct CreateContextFromType : public Packet
{
	CreateContextFromType() noexcept : Packet(PacketType::CreateContextFromType) {}

	uint64_t mDeviceType;
	Serialiseable<std::vector<uint64_t>, uint8_t> mProperties;
};

struct CreateContext : public Packet
{
	CreateContext() noexcept : Packet(PacketType::CreateContext) {}

	Serialiseable<std::vector<uint64_t>, uint8_t> mProperties;
	Serialiseable<std::vector<uint16_t>> mDevices;
};

struct GetImageFormats final : public Packet
{
	GetImageFormats() noexcept : Packet(PacketType::GetImageFormats) {}

	IDType mContextID;
	uint32_t mFlags;
	uint32_t mImageType;
};

using GetContextInfo = IDParamPair<PacketType::GetContextInfo>;

inline SocketStream& operator <<(SocketStream& o, const CreateContextFromType& p)
{
	o << p.mDeviceType;
	o << p.mProperties;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateContextFromType& p)
{
	i >> p.mDeviceType;
	i >> p.mProperties;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const CreateContext& p)
{
	o << p.mProperties;
	o << p.mDevices;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateContext& p)
{
	i >> p.mProperties;
	i >> p.mDevices;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const GetImageFormats& p)
{
	o << p.mContextID;
	o << p.mFlags;
	o << p.mImageType;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, GetImageFormats& p)
{
	i >> p.mContextID;
	i >> p.mFlags;
	i >> p.mImageType;
	return i;
}

}

#endif

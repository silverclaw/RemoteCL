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

#if !defined(REMOTECL_PACKET_QUEUE_H)
#define REMOTECL_PACKET_QUEUE_H

#include <vector>

#include "idtype.h"
#include "packets/packet.h"
#include "streamserialise.h"

namespace RemoteCL
{
struct CreateQueue final : public Packet
{
	CreateQueue() noexcept : Packet(PacketType::CreateQueue) {}

	uint32_t mProp;
	IDType mContext;
	IDType mDevice;
};
struct CreateQueueWithProp final : public Packet
{
	CreateQueueWithProp() noexcept : Packet(PacketType::CreateQueueWithProp) {}

	IDType mContext;
	IDType mDevice;
	Serialiseable<std::vector<uint64_t>, uint8_t> mProperties;
};

using QFinishPacket = SimplePacket<PacketType::Finish, IDType>;
using QFlushPacket = SimplePacket<PacketType::Flush, IDType>;

SocketStream& operator<<(SocketStream& stream, const CreateQueue& packet)
{
	stream << packet.mContext;
	stream << packet.mDevice;
	stream << packet.mProp;
	return stream;
}

SocketStream& operator>>(SocketStream& stream, CreateQueue& packet)
{
	stream >> packet.mContext;
	stream >> packet.mDevice;
	stream >> packet.mProp;
	return stream;
}


SocketStream& operator<<(SocketStream& stream, const CreateQueueWithProp& packet)
{
	stream << packet.mContext;
	stream << packet.mDevice;
	stream << packet.mProperties;
	return stream;
}

SocketStream& operator>>(SocketStream& stream, CreateQueueWithProp& packet)
{
	stream >> packet.mContext;
	stream >> packet.mDevice;
	stream >> packet.mProperties;
	return stream;
}

}

#endif

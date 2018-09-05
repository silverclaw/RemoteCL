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

#if !defined(REMOTECL_PACKET_IDS_H)
#define REMOTECL_PACKET_IDS_H
/// @file IDs.h Adds packets to transfer object IDs.

#include <vector>

#include "idtype.h"
#include "packets/packet.h"
#include "socketstream.h"
#include "streamserialise.h"

namespace RemoteCL
{
/// Transfers a list of object IDs.
struct IDListPacket final : public Packet
{
	IDListPacket() noexcept : Packet(PacketType::IDList) {}

	/// List of platforms to be returned, in mapped form.
	Serialiseable<std::vector<IDType>, uint8_t> mIDs;
};

/// Transfers a single object ID.
using IDPacket = SimplePacket<PacketType::ID, IDType>;

/// Pairs an ID with some other type.
/// @tparam Type The packet type used for the packet.
/// @tparam T The payload type.
template<PacketType Type, typename T>
struct IDTypePair final : public Packet
{
	IDTypePair() noexcept : Packet(Type) {}
	IDTypePair(IDType id, T data) noexcept :
		Packet(Type), mData(data), mID(id) {}

	/// The data payload name.
	T mData;
	/// The object ID for the query.
	IDType mID;
};

/// Used in GetInfo queries where an ID and a parameter are used.
/// @tparam Type The packet type used for the query.
template<PacketType Type>
using IDParamPair = IDTypePair<Type, uint32_t>;

inline SocketStream& operator <<(SocketStream& o, const IDListPacket& list)
{
	return o << list.mIDs;
}

inline SocketStream& operator >>(SocketStream& i, IDListPacket& list)
{
	return i >> list.mIDs;
}

template<PacketType Type, typename T>
SocketStream& operator <<(SocketStream& o, const IDTypePair<Type, T>& pair)
{
	return o << pair.mData << pair.mID;
}

template<PacketType Type, typename T>
SocketStream& operator >>(SocketStream& i, IDTypePair<Type, T>& pair)
{
	i >> pair.mData;
	return i >> pair.mID;
}
}

#endif

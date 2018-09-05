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

#if !defined(REMOTECL_PACKET_SIMPLE_H)
#define REMOTECL_PACKET_SIMPLE_H
/// @file simple.h Defines simple packet types.

#include <cstdint>

#include "packets/packet.h"

namespace RemoteCL
{
/// A simple packet contains a simple payload.
template<PacketType Type, typename T>
struct SimplePacket final : public Packet
{
	SimplePacket() noexcept : Packet(Type) {}
	SimplePacket(T init) noexcept : Packet(Type), mData(init) {}
	operator T() const noexcept { return mData; }
	T mData;
};

template<PacketType Type, typename T>
SocketStream& operator <<(SocketStream& o, const SimplePacket<Type, T>& packet)
{
	o << packet.mData;
	return o;
}

template<PacketType Type, typename T>
SocketStream& operator >>(SocketStream& i, SimplePacket<Type, T>& packet)
{
	i >> packet.mData;
	return i;
}

using ErrorPacket = SimplePacket<PacketType::Error, int32_t>;
/// De-serialises an error Packet and throws it as an expection.
inline void ThrowErrorPacket(SocketStream& i)
{
	RemoteCL::ErrorPacket e;
	i >> e;
	throw e;
}


/// A signal packet contains no payload.
template<PacketType Type>
struct SignalPacket final : public Packet
{
	SignalPacket() noexcept : Packet(Type) {}
};

template<PacketType Type>
SocketStream& operator <<(SocketStream& o, const SignalPacket<Type>&) noexcept
{
	return o;
}

template<PacketType Type>
SocketStream& operator >>(SocketStream& i, SignalPacket<Type>&) noexcept
{
	return i;
}

/// Some commands simply return that they succeeded.
using SuccessPacket = SignalPacket<PacketType::Success>;
}

#endif // REMOTECL_PACKET_SIMPLE_H

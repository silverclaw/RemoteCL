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

#if !defined(REMOTECL_PACKET_EVENT_H)
#define REMOTECL_PACKET_EVENT_H
/// @file event.h Defines event packet types.

#include <cstdint>

#include "idtype.h"
#include "packets/simple.h"
#include "packets/IDs.h"

namespace RemoteCL
{
template<PacketType Type>
struct EventCallback final : public Packet
{
	EventCallback() noexcept : Packet(Type) {}

	// unique callback ID
	uint32_t mID;

	IDType mEventID;
	int32_t mCallbackType;
};

using CreateUserEvent = SimplePacket<PacketType::CreateUserEvent, IDType>;
using SetUserEventStatus = IDTypePair<PacketType::SetUserEventStatus, uint32_t>;
using GetEventInfo = IDParamPair<PacketType::GetEventInfo>;
using GetEventProfilingInfo = IDParamPair<PacketType::GetEventProfilingInfo>;
using SetEventCallback = EventCallback<PacketType::SetEventCallback>;
using FireEventCallback = SimplePacket<PacketType::FireEventCallback, uint32_t>;
using WaitForEvents = SignalPacket<PacketType::WaitEvents>;

template<PacketType Type>
inline SocketStream& operator <<(SocketStream& o, const EventCallback<Type>& E)
{
	o << E.mID;
	o << E.mEventID;
	o << E.mCallbackType;

	return o;
}

template<PacketType Type>
inline SocketStream& operator >>(SocketStream& i, EventCallback<Type>& E)
{
	i >> E.mID;
	i >> E.mEventID;
	i >> E.mCallbackType;

	return i;
}
}

#endif

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

#if !defined(REMOTECL_PACKET_CALLBACKS_H)
#define REMOTECL_PACKET_CALLBACKS_H
/// @file callbacks.h Defines event packet types.

#include <cstdint>

#include "idtype.h"
#include "packets/simple.h"
#include "packets/IDs.h"

namespace RemoteCL
{
using OpenEventStream = SignalPacket<PacketType::EventStreamOpen>;
/// Triggers a predefined callback.
using CallbackTriggerPacket = SimplePacket<PacketType::CallbackTrigger, uint32_t>;
using TriggerEventCallback = SimplePacket<PacketType::EventCallbackTrigger, IDType>;

struct RegisterEventCallback : public Packet
{
    RegisterEventCallback() :
        Packet(PacketType::RegisterEventCallback)
    {}

    IDType mEventID;
    /// Client-side ID for the callback.
    IDType mCallbackID;
    /// The callback event type.
    uint32_t mCBType;
};

inline SocketStream& operator <<(SocketStream& o, const RegisterEventCallback& P) noexcept
{
    o << P.mEventID;
    o << P.mCallbackID;
    o << P.mCBType;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, RegisterEventCallback& P) noexcept
{
    i >> P.mEventID;
    i >> P.mCallbackID;
    i >> P.mCBType;
	return i;
}
}

#endif

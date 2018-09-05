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

#if !defined(REMOTECL_PACKET_PLATFORM_H)
#define REMOTECL_PACKET_PLATFORM_H

#include "packets/packet.h"
#include "packets/IDs.h"

namespace RemoteCL
{
using GetPlatformIDs = SignalPacket<PacketType::GetPlatformIDs>;
using GetPlatformInfo = IDParamPair<PacketType::GetPlatformInfo>;
}

#endif

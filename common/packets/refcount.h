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

#if !defined(REMOTECL_PACKET_REFCOUNT_H)
#define REMOTECL_PACKET_REFCOUNT_H

#include "idtype.h"
#include "packets/packet.h"

namespace RemoteCL
{
template<PacketType Ty>
struct RefCount : public Packet
{
	RefCount() noexcept : Packet(Ty) {}
	RefCount(char objTy, IDType id) noexcept : Packet(Ty), mID(id), mObjTy(objTy) {}

	IDType mID = 0;
	char mObjTy = 'U';
};

using Retain = RefCount<PacketType::Retain>;
using Release = RefCount<PacketType::Release>;

template<PacketType Type>
SocketStream& operator <<(SocketStream& o, const RefCount<Type>& p)
{
	o << p.mObjTy;
	o << p.mID;
	return o;
}

template<PacketType Type>
SocketStream& operator >>(SocketStream& i, RefCount<Type>& p)
{
	i >> p.mObjTy;
	i >> p.mID;
	return i;
}
}

#endif

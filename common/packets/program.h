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

#if !defined(REMOTECL_PACKET_PROGRAM_H)
#define REMOTECL_PACKET_PROGRAM_H

#include "streamserialise.h"
#include "packets/packet.h"
#include "packets/IDs.h"

namespace RemoteCL
{
template<PacketType Type>
struct IDStringPair final : public Packet
{
	IDStringPair() noexcept : Packet(Type) {}
	IDType mID;
	std::string mString;
};

struct KernelArg final : public Packet
{
	KernelArg() : Packet(PacketType::SetKernelArg) {}

	/// The argument index being set.
	uint32_t mArgIndex;
	/// The kernel ID for this argument.
	IDType mKernelID;
};

struct KernelArgInfo final : public Packet
{
	KernelArgInfo() : Packet(PacketType::KernelArgInfo) {}

	/// The argument index being set.
	uint32_t mArgIndex;
	/// What is being queried.
	uint32_t mParam;
	/// The kernel ID for this argument.
	IDType mKernelID;
};

struct KernelWGInfo final : public Packet
{
	KernelWGInfo() : Packet(PacketType::KernelWGInfo) {}

	/// The kernel ID for this argument.
	IDType mKernelID;
	IDType mDeviceID;
	uint32_t mParam;
};

using BinaryProgram = SimplePacket<PacketType::CreateBinaryProgram, IDType>;
using ProgramSource = IDStringPair<PacketType::CreateSourceProgram>;
using KernelName = IDStringPair<PacketType::CreateKernel>;
using BuildProgram = IDStringPair<PacketType::BuildProgram>;
using ProgramInfo = IDParamPair<PacketType::ProgramInfo>;
using KernelInfo = IDParamPair<PacketType::KernelInfo>;

struct ProgramBuildInfo final : public Packet
{
	ProgramBuildInfo() : Packet(PacketType::BuildInfo) {}

	uint32_t mParam;
	IDType mProgramID;
	IDType mDeviceID;
};

template<PacketType Type>
inline SocketStream& operator <<(SocketStream& o, const IDStringPair<Type>& p)
{
	o << p.mID;
	o << p.mString;
	return o;
}

template<PacketType Type>
inline SocketStream& operator >>(SocketStream& i, IDStringPair<Type>& p)
{
	i >> p.mID;
	i >> p.mString;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const ProgramBuildInfo& p)
{
	o << p.mParam;
	o << p.mProgramID;
	o << p.mDeviceID;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, ProgramBuildInfo& p)
{
	i >> p.mParam;
	i >> p.mProgramID;
	i >> p.mDeviceID;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const KernelArg& arg)
{
	o << arg.mKernelID;
	o << arg.mArgIndex;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, KernelArg& arg)
{
	i >> arg.mKernelID;
	i >> arg.mArgIndex;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const KernelArgInfo& arg)
{
	o << arg.mKernelID;
	o << arg.mArgIndex;
	o << arg.mParam;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, KernelArgInfo& arg)
{
	i >> arg.mKernelID;
	i >> arg.mArgIndex;
	i >> arg.mParam;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const KernelWGInfo& arg)
{
	o << arg.mKernelID;
	o << arg.mDeviceID;
	o << arg.mParam;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, KernelWGInfo& arg)
{
	i >> arg.mKernelID;
	i >> arg.mDeviceID;
	i >> arg.mParam;
	return i;
}
}

#endif

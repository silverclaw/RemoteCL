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

struct CreateKernels final : public Packet
{
	CreateKernels() : Packet(PacketType::CreateKernels) {}

	/// The ID of the parent program.
	IDType mProgramID;
	/// The number of kernels to be created.
	uint32_t mKernelCount;
};

struct CompileProgram final : public Packet
{
	CompileProgram() : Packet(PacketType::CompileProgram) {}

	/// The ID of the parent program.
	IDType mProgramID;
	/// The build options specified.
	std::string mOptions;
	/// The list of target devices.
	Serialiseable<std::vector<IDType>, uint8_t> mDeviceIDs;
	/// The list of headers (cl_program IDs).
	Serialiseable<std::vector<IDType>, uint8_t> mHeaderIDs;
	/// The list of header names.
	Serialiseable<std::vector<std::string>, uint8_t> mHeaderNames;
	/// Has a callback been registered.
	bool mHasCallback = false;
	/// The registered callback ID.
	IDType mCallbackID;
};

struct LinkProgram final : public Packet
{
	LinkProgram() : Packet(PacketType::LinkProgram) {}

	/// The parent context.
	IDType mContext;
	/// The list of programs to link.
	Serialiseable<std::vector<IDType>, uint8_t> mProgramIDs;
	/// The list of target devices.
	Serialiseable<std::vector<IDType>, uint8_t> mDeviceIDs;
	/// The build options specified.
	std::string mOptions;
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

inline SocketStream& operator <<(SocketStream& o, const CreateKernels& arg)
{
	o << arg.mProgramID;
	o << arg.mKernelCount;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateKernels& arg)
{
	i >> arg.mProgramID;
	i >> arg.mKernelCount;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const CompileProgram& arg)
{
	o << arg.mProgramID;
	o << arg.mOptions;
	o << arg.mDeviceIDs;
	o << arg.mHeaderIDs;
	o << arg.mHeaderNames;
	o << arg.mHasCallback;
	if (arg.mHasCallback)
		o << arg.mCallbackID;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, CompileProgram& arg)
{
	i >> arg.mProgramID;
	i >> arg.mOptions;
	i >> arg.mDeviceIDs;
	i >> arg.mHeaderIDs;
	i >> arg.mHeaderNames;
	i >> arg.mHasCallback;
	if (arg.mHasCallback)
		i >> arg.mCallbackID;
	return i;
}

inline SocketStream& operator <<(SocketStream& o, const LinkProgram& arg)
{
	o << arg.mContext;
	o << arg.mOptions;
	o << arg.mDeviceIDs;
	o << arg.mProgramIDs;
	return o;
}

inline SocketStream& operator >>(SocketStream& i, LinkProgram& arg)
{
	i >> arg.mContext;
	i >> arg.mOptions;
	i >> arg.mDeviceIDs;
	i >> arg.mProgramIDs;
	return i;
}
}

#endif

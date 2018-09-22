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

#if !defined(REMOTECL_PACKET_H)
#define REMOTECL_PACKET_H
/// @file packet.h

#include <cstdint>
#include <cassert>

#include "socketstream.h"

namespace RemoteCL
{
/// Describe the packet types that can be exchanged over the connection.
enum class PacketType : uint8_t
{
	/// Encodes the version of the protocol.
	Version,

	/// Some undetermined data burst.
	Payload,

	/// The command was completed successfully (and no return was expected).
	Success,
	/// Sent when a command fails. The client throws an exception when this is received.
	Error,

	// Reference count manipulation.
	Retain,
	Release,

	// Context functions.
	CreateContext,
	CreateContextFromType,
	GetContextInfo,
	GetImageFormats,

	// Command Queue functions.
	CreateQueue,
	CreateQueueWithProp,
	GetQueueInfo,
	Flush,
	Finish,

	// Program functions.
	CreateSourceProgram,
	CreateBinaryProgram,
	BuildProgram,
	BuildInfo,
	ProgramInfo,

	// Kernel functions.
	CreateKernel,
	CloneKernel,
	SetKernelArg,
	KernelWGInfo,
	KernelInfo,
	KernelArgInfo,

	// Buffer functions.
	CreateBuffer,
	CreateSubBuffer,
	ReadBuffer,
	WriteBuffer,
	FillBuffer,
	GetMemObjInfo,

	// Image functions.
	CreateImage,
	ReadImage,
	WriteImage,
	GetImageInfo,

	// Commands.
	EnqueueKernel,

	// Events.
	CreateUserEvent,
	SetUserEventStatus,
	WaitEvents,

	// Platform functions.
	GetPlatformInfo,
	/// The request to fetch platform IDs.
	GetPlatformIDs,

	// Device functions.
	/// The request to fetch device IDs.
	GetDeviceIDs,
	GetDeviceInfo,

	/// A single ID.
	ID,
	/// A list of IDs, used to query platform and device IDs.
	IDList,

	// Signals the server that the connection is about to be terminated.
	Terminate = 0xFFu
};

inline SocketStream& operator <<(SocketStream& o, const PacketType& ty)
{
	return o << static_cast<uint8_t>(ty);
}

inline SocketStream& operator >>(SocketStream& i, PacketType& ty)
{
	uint8_t e;
	i >> e;
	ty = static_cast<PacketType>(e);
	return i;
}

/// The base Packet type, for all Packet types.
/// This type is not meant to be serialiseable, as it is considered incomplete.
struct Packet
{
	Packet(PacketType ty) noexcept : mType(ty) {}
	const PacketType mType;
};

}

#endif

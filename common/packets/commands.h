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

#if !defined(REMOTECL_PACKET_COMMANDS_H)
#define REMOTECL_PACKET_COMMANDS_H
/// @file commands.h Defines packets for commands.

#include <array>

#include "idtype.h"
#include "packets/packet.h"
#include "socketstream.h"
#include "streamserialise.h"

namespace RemoteCL
{
struct EnqueueKernel final : public Packet
{
	EnqueueKernel() noexcept : Packet(PacketType::EnqueueKernel) {}

	std::array<uint32_t, 3> mGlobalSize;
	std::array<uint32_t, 3> mGlobalOffset;
	std::array<uint32_t, 3> mLocalSize;

	IDType mKernelID;
	IDType mQueueID;
	uint8_t mWorkDim;
	bool mWantEvent = false;
	bool mExpectEventList = false;
};

template<PacketType Type>
struct ImageRW final : public Packet
{
	ImageRW() noexcept : Packet(Type) {}

	IDType mImageID;
	IDType mQueueID;
	uint32_t mRowPitch, mSlicePitch;
	std::array<uint32_t, 3> mOrigin;
	std::array<uint32_t, 3> mRegion;
	bool mWantEvent = false;
	bool mExpectEventList = false;
	bool mBlock;
};

using ReadImage = ImageRW<PacketType::ReadImage>;
using WriteImage = ImageRW<PacketType::WriteImage>;

template<PacketType Type>
struct BufferRW final : public Packet
{
	BufferRW() noexcept : Packet(Type) {}

	IDType mBufferID;
	IDType mQueueID;
	uint32_t mSize;
	uint32_t mOffset;
	bool mWantEvent = false;
	bool mExpectEventList = false;
	bool mBlock;
};

using ReadBuffer = BufferRW<PacketType::ReadBuffer>;
using WriteBuffer = BufferRW<PacketType::WriteBuffer>;

struct FillBuffer final : public Packet
{
	FillBuffer() noexcept : Packet(PacketType::FillBuffer) {}

	IDType mBufferID;
	IDType mQueueID;
	uint32_t mOffset;
	uint32_t mSize;
	uint8_t mPatternSize;
	bool mWantEvent = false;
	bool mExpectEventList = false;
	// The maximum primitive size in OpenCL is a double16 (or long16).
	// That's 8 bytes * vec16, thus 128 bytes.
	std::array<uint8_t, 128> mPattern;
};

inline SocketStream& operator <<(SocketStream& o, const EnqueueKernel& E)
{
	o << E.mKernelID;
	o << E.mQueueID;
	o << E.mWorkDim;

	o << E.mGlobalSize;
	o << E.mGlobalOffset;
	o << E.mLocalSize;

	o << E.mWantEvent;
	o << E.mExpectEventList;

	return o;
}

inline SocketStream& operator >>(SocketStream& i, EnqueueKernel& E)
{
	i >> E.mKernelID;
	i >> E.mQueueID;
	i >> E.mWorkDim;

	i >> E.mGlobalSize;
	i >> E.mGlobalOffset;
	i >> E.mLocalSize;

	i >> E.mWantEvent;
	i >> E.mExpectEventList;

	return i;
}

template<PacketType Type>
SocketStream& operator <<(SocketStream& o, const ImageRW<Type>& E)
{
	o << E.mImageID;
	o << E.mQueueID;

	o << E.mOrigin;
	o << E.mRegion;

	o << E.mRowPitch;
	o << E.mSlicePitch;

	o << E.mWantEvent;
	o << E.mExpectEventList;
	o << E.mBlock;

	return o;
}

template<PacketType Type>
SocketStream& operator >>(SocketStream& i, ImageRW<Type>& E)
{
	i >> E.mImageID;
	i >> E.mQueueID;

	i >> E.mOrigin;
	i >> E.mRegion;

	i >> E.mRowPitch;
	i >> E.mSlicePitch;

	i >> E.mWantEvent;
	i >> E.mExpectEventList;
	i >> E.mBlock;

	return i;
}

template<PacketType Type>
SocketStream& operator <<(SocketStream& o, const BufferRW<Type>& E)
{
	o << E.mBufferID;
	o << E.mQueueID;
	o << E.mSize;
	o << E.mOffset;
	o << E.mWantEvent;
	o << E.mExpectEventList;
	o << E.mBlock;

	return o;
}

template<PacketType Type>
SocketStream& operator >>(SocketStream& i, BufferRW<Type>& E)
{
	i >> E.mBufferID;
	i >> E.mQueueID;
	i >> E.mSize;
	i >> E.mOffset;
	i >> E.mWantEvent;
	i >> E.mExpectEventList;
	i >> E.mBlock;

	return i;
}

inline SocketStream& operator <<(SocketStream& o, const FillBuffer& E)
{
	o << E.mBufferID;
	o << E.mQueueID;
	o << E.mSize;
	o << E.mOffset;
	o << E.mPatternSize;
	o << E.mWantEvent;
	o << E.mExpectEventList;
	o << E.mPattern;

	return o;
}

inline SocketStream& operator >>(SocketStream& i, FillBuffer& E)
{
	i >> E.mBufferID;
	i >> E.mQueueID;
	i >> E.mSize;
	i >> E.mOffset;
	i >> E.mPatternSize;
	i >> E.mWantEvent;
	i >> E.mExpectEventList;
	i >> E.mPattern;

	return i;
}

}

#endif

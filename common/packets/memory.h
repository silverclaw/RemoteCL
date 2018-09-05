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

#if !defined(REMOTECL_PACKET_MEMORY_H)
#define REMOTECL_PACKET_MEMORY_H
/// @file memory.h Defines packets for memory object manipulation.

#include "idtype.h"
#include "packets/packet.h"

namespace RemoteCL
{
struct CreateBuffer final : public Packet
{
	CreateBuffer() : Packet(PacketType::CreateBuffer) {}

	uint32_t mFlags;
	uint32_t mSize;

	IDType mContextID;
	bool mExpectPayload = false;
};

struct CreateSubBuffer final : public Packet
{
	CreateSubBuffer() : Packet(PacketType::CreateSubBuffer) {}

	uint32_t mFlags;
	uint32_t mSize;
	uint32_t mOffset;
	uint32_t mCreateType;

	IDType mBufferID;
};

struct CreateImage final : public Packet
{
	CreateImage() : Packet(PacketType::CreateImage) {}

	uint32_t mFlags;
	uint32_t mChannelOrder;
	uint32_t mChannelType;
	uint32_t mImageType;
	uint32_t mWidth;
	uint32_t mHeight;
	uint32_t mDepth;
	uint32_t mArraySize;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
	uint32_t mMipLevels;
	uint32_t mSamples;

	IDType mContextID;
};

inline SocketStream& operator <<(SocketStream& o, const CreateBuffer& b)
{
	o << b.mFlags;
	o << b.mSize;
	o << b.mContextID;
	o << b.mExpectPayload;

	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateBuffer& b)
{
	i >> b.mFlags;
	i >> b.mSize;
	i >> b.mContextID;
	i >> b.mExpectPayload;

	return i;
}

inline SocketStream& operator <<(SocketStream& o, const CreateSubBuffer& b)
{
	o << b.mFlags;
	o << b.mSize;
	o << b.mOffset;
	o << b.mCreateType;
	o << b.mBufferID;

	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateSubBuffer& b)
{
	i >> b.mFlags;
	i >> b.mSize;
	i >> b.mOffset;
	i >> b.mCreateType;
	i >> b.mBufferID;

	return i;
}

inline SocketStream& operator <<(SocketStream& o, const CreateImage& img)
{
	o << img.mFlags;
	o << img.mChannelOrder;
	o << img.mChannelType;
	o << img.mImageType;
	o << img.mWidth;
	o << img.mHeight;
	o << img.mDepth;
	o << img.mArraySize;
	o << img.mRowPitch;
	o << img.mSlicePitch;
	o << img.mMipLevels;
	o << img.mSamples;
	o << img.mContextID;

	return o;
}

inline SocketStream& operator >>(SocketStream& i, CreateImage& img)
{
	i >> img.mFlags;
	i >> img.mChannelOrder;
	i >> img.mChannelType;
	i >> img.mImageType;
	i >> img.mWidth;
	i >> img.mHeight;
	i >> img.mDepth;
	i >> img.mArraySize;
	i >> img.mRowPitch;
	i >> img.mSlicePitch;
	i >> img.mMipLevels;
	i >> img.mSamples;
	i >> img.mContextID;

	return i;
}

}

#endif

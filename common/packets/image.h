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

#if !defined(REMOTECL_PACKET_IMAGE_H)
#define REMOTECL_PACKET_IMAGE_H
/// @file image.h Defines packets for image object manipulation.

#include "idtype.h"
#include "packets/packet.h"

namespace RemoteCL
{
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

#endif // REMOTECL_PACKET_IMAGE_H

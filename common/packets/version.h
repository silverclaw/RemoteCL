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

#if !defined(REMOTECL_PACKET_VERSION_H)
#define REMOTECL_PACKET_VERSION_H
/// @file version.h

#include <cstdint>

#include "idtype.h"
#include "packets/packet.h"

namespace RemoteCL
{
/// Defines a packet sent by the server annotated with the
/// exposed CL version and server capabilities.
struct VersionPacket : public Packet
{
	VersionPacket() noexcept : Packet(PacketType::Version)
	{
		std::size_t i = 0;
		mVersion[i++] = REMOTECL_VERSION_MAJ;
		mVersion[i++] = REMOTECL_VERSION_MIN;
		mVersion[i++] = ' ';
		mVersion[i++] = sizeof(IDType);

		assert(i == SWVersionSize);

		// Append any enabled features
#if defined(REMOTECL_USE_ZLIB)
		mVersion[i++] = 'z';
#endif
#if defined(REMOTECL_ENABLE_ASYNC)
		mVersion[i++] = 'e';
#endif
		mVersion[i++] = '\0';
	}

	/// Checks if these versions are compatible.
	bool isCompatibleWith(const VersionPacket& v) const noexcept;

	/// Checks if this version packet contains the server back-connection enabled.
	bool eventEnabled() const noexcept;
	/// Checks if the compression feature is enabled.
	bool compressionEnabled() const noexcept;

	// Allow a total of 64 bytes to encode RemoteCL version and features.
	// This should be sufficient. If more data is required in the future, a second
	// version packet must be sent.
	uint8_t mVersion[64] = {0};

private:
	/// Number of characters used for the SW version part of the packet.
	static constexpr std::size_t SWVersionSize = 4;
};

inline SocketStream& operator <<(SocketStream& o, const VersionPacket& v)
{
	o.write(v.mVersion, sizeof(v.mVersion));
	return o;
}

inline SocketStream& operator >>(SocketStream& i, VersionPacket& v)
{
	i.read(v.mVersion, sizeof(v.mVersion));
	return i;
}

}

#endif

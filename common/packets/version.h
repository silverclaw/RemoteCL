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
		// Current version 0.1
		mVersion[i++] = '0';
		mVersion[i++] = '.';
		mVersion[i++] = '1';
		mVersion[i++] = ' ';
		mVersion[i++] = CL_TARGET_OPENCL_VERSION;
		mVersion[i++] = sizeof(IDType);

		// Append any enabled features
#if defined(REMOTECL_USE_ZLIB)
		mVersion[i++] = 'z';
#endif
		mVersion[i++] = '\0';
	}

	/// Checks if these versions are compatible.
	bool isCompatibleWith(const VersionPacket& v) const noexcept
	{
		// Client/server versions must match
		std::size_t i = 0;
		do {
			if (mVersion[i] != v.mVersion[i]) {
				return false;
			}
			++i;
		} while (mVersion[i] != ' ' && v.mVersion[i] != ' ');
		++i;

		// After the blank space, the targeted CL version is encoded.
		// "v" will be the server version; a server may have a target OpenCL version greater
		// than the client's (this). The other way around however, is not allowed.
		// Thus, if the client version is greater than the server's, fail.
		// In theory, we could return CL_INVALID_OPERATION from the server if the client
		// uses a server-unsupported operation though.
		if (mVersion[i] > v.mVersion[i]) return false;
		++i;

		// Next is the size in bytes of the type used for Object IDs, which must match.
		// In theory we could allow a different size, but we'd have to adjust the (de)serialiser
		// of any packets that use IDType. Too much hassle; just reject different sizes.
		if (mVersion[i] != v.mVersion[i]) return false;
		++i;

		// Additional features enabled must match.
		// At current version, all enabled additional features must match.
		do {
			if (mVersion[i] != v.mVersion[i]) {
				return false;
			}
			++i;
		} while (mVersion[i] != '\0' && v.mVersion[i] != '\0');

		return true;
	}

	// Allow a total of 64 bytes to encode RemoteCL version and features.
	// This should be sufficient. If more data is required in the future, a second
	// version packet must be sent.
	uint8_t mVersion[64] = {0};
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

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

#if !defined(REMOTECL_PACKETSTREAM_H)
#define REMOTECL_PACKETSTREAM_H
/// @file packetstream.h

#include "packets/packet.h"
#include "packets/simple.h"
#include "socketstream.h"

namespace RemoteCL
{
/// Transfers packets across a socket.
class PacketStream
{
public:
	PacketStream(Socket socket) : mStream(std::move(socket)) {}

	template<typename PacketTy>
	void read(PacketTy&& packet)
	{
		PacketType ty; mStream >> ty;
		if (ty == PacketType::Error) {
			RemoteCL::ErrorPacket e;
			mStream >> e;
			throw e;
		} else if (ty == PacketType::Terminate) {
			// The stream generates this packet when the socket closes.
			throw Socket::Error();
		}
		assert(ty == packet.mType);
		mStream >> packet;
	}

	/// Reads the incoming packet.
	/// @tparam PacketTy The Packet object that is expected to arrive.
	template<typename PacketTy>
	PacketTy read()
	{
		PacketTy packet;
		read(packet);
		return packet;
	}

	/// Writes this packet out into the stream.
	template<typename PacketTy>
	PacketStream& write(const PacketTy& p)
	{
		mStream << p.mType;
		mStream << p;
		return *this;
	}

	/// Blocks until there is an incoming packet and returns its type.
	PacketType nextPacketTy() noexcept
	{
		auto ty = mStream.peek();
		if (ty == -1) return PacketType::Terminate;
		return static_cast<PacketType>(ty);
	}

	void flush() { mStream.flush(); }

	void shutdown() { mStream.shutdown(); }

private:
	/// The underlying buffer for the socket.
	SocketStream mStream;
};
}

#endif

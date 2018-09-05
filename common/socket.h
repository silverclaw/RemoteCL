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

#if !defined(REMOTECL_SOCKET_H)
#define REMOTECL_SOCKET_H
/// @file socket.h

#include <exception>
#include <cstdint>
#include <utility> // for std::swap

#if defined(WIN32)
#include <WinSock2.h>
#endif

namespace RemoteCL
{
/// Wraps a TCP Stream socket.
class Socket
{
public:
#if !defined(WIN32)
	using SocketTy = int;
	static constexpr SocketTy InvalidSocket = -1;
#else
	using SocketTy = SOCKET;
	static constexpr SocketTy InvalidSocket = INVALID_SOCKET;
#endif

	enum
	{
		DefaultPort =
#if defined(REMOTECL_DEFAULT_PORT)
			REMOTECL_DEFAULT_PORT
#else
			23857
#endif
	};

	/// Thrown when a socket read or write operation failed.
	/// Usually, because the socket closed.
	class Error : public std::exception {};

	struct PeerName
	{
		char data[256];
	};

	/// Opens a server socket on this port.
	explicit Socket(uint16_t port);
	/// Opens a client socket to this hostname and port.
	explicit Socket(const char* hostname, uint16_t port);
	Socket(const Socket&) = delete;
	Socket(Socket&& o) noexcept { std::swap(o.mSocket, mSocket); }
	~Socket() noexcept { if (mSocket != -1) close(); }

	Socket& operator=(const Socket&) = delete;

	/// Accepts an incoming connection.
	Socket accept();
	/// Closes this socket.
	void close() noexcept;

	/// Send this data burst (blocing).
	void send(const void* data, std::size_t size);
	/// Receives at most these many bytes.
	/// @param data where the received bytes are stored.
	/// @param available Maximum number of bytes to receive.
	/// @returns the number of bytes received.
	std::size_t receive(void* data, std::size_t available);

	/// Returns the host name connected to this socket.
	PeerName getPeerName() noexcept;

private:
	Socket();
	explicit Socket(SocketTy socket) noexcept : mSocket(socket) {}

	SocketTy mSocket = InvalidSocket;
};

}

#endif

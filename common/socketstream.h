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

#if !defined(REMOTECL_SOCKETSTREAM_H)
#define REMOTECL_SOCKETSTREAM_H
/// @file socketstream.h

#include <type_traits>

#include "socket.h"

namespace RemoteCL
{
/// Wraps a network socket with a read/write cache.
/// Allows building a data burst to reduce the number of socket operations.
class SocketStream final
{
public:
	SocketStream(Socket socket) noexcept : mSocket(std::move(socket)) {}
	~SocketStream() noexcept = default;

	/// Size of the read and write buffers in bytes.
	static constexpr std::size_t BufferSize = 1024;

	/// Sets this data for output.
	void write(const void* s, std::size_t n);
	/// Blocks until these many bytes have been read.
	void read(void* s, std::size_t count);

	/// Flushes the writes.
	void flush() { if (mWriteOffset) { flushWriteBuffer(); } }

	/// Shut down the stream, no more reads/writes are possible.
	void shutdown() { mSocket.shutdown(); }

	/// How many characters available for non-blocking read.
	std::size_t available() const noexcept
	{
		return mAvailable;
	}

	/// Preview the next character available. Or -1 is there is no incoming data.
	char peek() noexcept
	{
		try {
			if (available() == 0) readMoreData();
			if (available() == 0) return -1;
		} catch (...) {
			return -1;
		}
		return mReadBuffer[mReadOffset];
	}

	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	SocketStream& operator<<(const T t)
	{
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Platform not supported - this isn't little endian"
#endif
		write(reinterpret_cast<const void*>(&t), sizeof(t));
		return *this;
	}

	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	SocketStream& operator>>(T& t)
	{
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Platform not supported - this isn't little endian"
// probably will need __builtin_bswap{16,32,64}
#endif
		read(reinterpret_cast<void*>(&t), sizeof(t));
		return *this;
	}

private:
	/// Fills as much of mBuffer as possible from the incoming socket.
	void readMoreData();

	/// Sends all pending data and resets the writeOffset;
	void flushWriteBuffer();

	/// Buffer for outgoing data. Unlike the ReadBuffer, the write buffer is not circular.
	char mWriteBuffer[BufferSize];
	/// Number of bytes left to be flushed out.
	uint16_t mWriteOffset = 0;

	/// Buffer for data waiting to be read.
	char mReadBuffer[BufferSize];
	/// Current offset into buffer where the read head is.
	uint16_t mReadOffset = 0;
	/// How many bytes are available for reading.
	uint16_t mAvailable = 0;

	/// The owned network socket.
	Socket mSocket;
};
}

#endif

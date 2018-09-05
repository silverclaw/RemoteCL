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

#include "socketstream.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace RemoteCL;

void SocketStream::read(void* source, std::size_t count)
{
	uint8_t* s = reinterpret_cast<uint8_t*>(source);

	while (count != 0) {
		if (mAvailable == 0) {
			if (count >= BufferSize) {
				// read directly into the output - no point in caching.
				const std::size_t bytesRead = mSocket.receive(s, count);
				s += bytesRead;
				count -= bytesRead;
				continue;
			}
			readMoreData();
			if (mAvailable == 0) throw Socket::Error();
		}

		assert(mAvailable <= BufferSize);
		assert(mAvailable != 0 && "Didn't we just try to read more data in?");

		/// The real data size we will be transferring on this iteration.
		const std::size_t readSize = std::min<std::size_t>(count, mAvailable);
		assert(readSize != 0 && "Must always read something");
		assert(readSize <= mAvailable && "std::min didn't work.");
		std::memcpy(s, &mReadBuffer[mReadOffset], readSize);
		s += readSize;
		mReadOffset += readSize;
		mAvailable -= readSize;
		count -= readSize;

		if (mReadOffset == BufferSize) {
			assert(mAvailable == 0);
		} else {
			assert(mReadOffset < BufferSize && "Went around the buffer.");
		}
	}
}

void SocketStream::readMoreData()
{
	assert(mAvailable == 0 && "You still have data to go through - read that first.");
	assert(mAvailable < BufferSize && "Buffer is already full - should not call this.");
	mReadOffset = 0;
	const std::size_t bytesRead = mSocket.receive(mReadBuffer, BufferSize);
	assert(bytesRead <= BufferSize && "received out of buffer.");
	mAvailable = bytesRead;
}

void SocketStream::write(const void* out, std::size_t n)
{
	const uint8_t* s = reinterpret_cast<const uint8_t*>(out);

	// No point in buffering the output if we'd have to flush straight away.
	if (static_cast<std::size_t>(n) >= BufferSize) {
		if (mWriteOffset != 0) flushWriteBuffer();
		// Just write out the whole output directly into the socket.
		mSocket.send(s, n);
		return;
	}

	/// How many bytes are left to be written.
	std::size_t leftToWrite = n;
	// This will loop at most twice, since a write larger than the buffer
	// will have already been written out to the socket.
	while (leftToWrite != 0) {
		/// Available size on the output buffer.
		const std::size_t available = BufferSize - mWriteOffset;
		/// How much data we can write in this go:
		const std::size_t writeSize = std::min<std::size_t>(available, leftToWrite);

		// Write as much data as possible in this iteration.
		std::memcpy(&mWriteBuffer[mWriteOffset], s, writeSize);
		mWriteOffset += writeSize;
		assert(mWriteOffset <= BufferSize && "Wrote past end of WriteBuffer");
		// Advance source buffer.
		s += writeSize;

		// Ensure that we don't leave the buffer completely full.
		if (mWriteOffset == BufferSize) {
			flushWriteBuffer();
		}

		assert(leftToWrite >= writeSize && "Wrote past the end of buffer?");
		leftToWrite -= writeSize;
	}
}

void RemoteCL::SocketStream::flushWriteBuffer()
{
	assert(mWriteOffset != 0);
	mSocket.send(mWriteBuffer, mWriteOffset);
	mWriteOffset = 0;
}

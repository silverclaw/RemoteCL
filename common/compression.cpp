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
#if 0
// This file is not used by the RemoteCL project, as the SocketStream
// code no longer uses a stream compression, instead the payloads being compressed themselves.
// This file was left in the state where it was last used (before being replaced by
// inline zlib wrappers in compression.h). The associated code for CompressedSocket has
// been removed already, so this file might be deleted at some point.
#include "compression.h"

#include <zlib.h>
#include <exception>

#include "hints.h"

namespace
{
/// Wraps input ZStream (compression).
class IZStream
{
public:
	IZStream() noexcept
	{
		mStream.zalloc = Z_NULL;
		mStream.zfree = Z_NULL;
		mStream.opaque = Z_NULL;
		// Seems overkill to throw here as we'll return an empty vector instead.
		mOK = deflateInit(&mStream, Z_DEFAULT_COMPRESSION) == Z_OK;
	}

	operator bool() noexcept { return mOK; }

	operator z_stream*() noexcept { return &mStream; }

	z_stream* operator->() noexcept { return &mStream; }

	~IZStream()
	{
		deflateEnd(&mStream);
	}
private:
	bool mOK;
	z_stream mStream;
};

/// Wraps output ZStream (decompression).
class OZStream
{
public:
	OZStream()
	{
		mStream.zalloc = Z_NULL;
		mStream.zfree = Z_NULL;
		mStream.opaque = Z_NULL;
		if (inflateInit(&mStream) != Z_OK)
			throw std::exception();
	}

	operator z_stream*() noexcept { return &mStream; }

	z_stream* operator->() noexcept { return &mStream; }

	~OZStream()
	{
		inflateEnd(&mStream);
	}
private:
	z_stream mStream;
};
}

/// Compress this input buffer.
std::vector<uint8_t> RemoteCL::Compress(const void* data, std::size_t len) noexcept
{
	try {
		IZStream stream;
		if (Unlikely(!stream)) {
			return std::vector<uint8_t>();
		}

		stream->avail_in = len;
		// Unfortunately, zlib isn't const-correct.
		void* nonConstData = const_cast<void*>(data);
		stream->next_in = reinterpret_cast<decltype(stream->next_in)>(nonConstData);
		std::vector<uint8_t> out;
		const std::size_t guessCompressedSize = deflateBound(stream, len);
		out.resize(guessCompressedSize);
		stream->avail_out = guessCompressedSize;
		stream->next_out = out.data();

		const int result = deflate(stream, Z_FINISH);
		// zlib guarantees result == Z_STREAM_END if the outsize was calculated by "deflateBound".
		if (Unlikely(result != Z_STREAM_END)) {
			return std::vector<uint8_t>();
		}

		// Trim the compressed vector to the compressed data.
		out.resize(guessCompressedSize - stream->avail_out);

		return out;
	} catch (...) {
		// Return an empty vector which will be handled.
		return std::vector<uint8_t>();
	}
}

/// Decompress this buffer.
void RemoteCL::Decompress(const void* data, std::size_t inLen, void* out, std::size_t outLen)
{
	OZStream stream;
	if (Unlikely(!stream)) {
		throw std::exception();
	}

	stream->avail_in = inLen;
	stream->next_in = reinterpret_cast<decltype(stream->next_in)>(const_cast<void*>(data));
	stream->avail_out = outLen;
	stream->next_out = reinterpret_cast<decltype(stream->next_out)>(out);
	const int r = inflate(stream, Z_FINISH);
	const bool outIsBad = (r != Z_STREAM_END || stream->avail_out != 0);
	if (Unlikely(outIsBad)) {
		// Output buffer isn't valid in this case.
		throw std::exception();
	}
	return;
}
#endif

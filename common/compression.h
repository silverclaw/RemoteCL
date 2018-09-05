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

#if !defined(REMOTECL_COMPRESSION_H)
/// @file compression.h Defines ZLib wrappers.
#include <cstdint>
#include <vector>
#include <zlib.h>
#include <vector>

#include "hints.h"


namespace RemoteCL
{
/// Compress this input buffer.
inline std::vector<uint8_t> Compress(const void* data, std::size_t len)
{
	uLongf compressedSize = compressBound(len);
	std::vector<uint8_t> out;
	out.resize(compressedSize);
	const int result = compress(out.data(), &compressedSize, reinterpret_cast<const Bytef*>(data), len);
	if (Unlikely(result != Z_OK)) {
		return std::vector<uint8_t>();
	}
	out.resize(compressedSize);

	return out;
}


/// Decompress this buffer.
inline void Decompress(const void* data, std::size_t inLen, void* out, std::size_t outLen)
{
	uLongf decompressedSize = outLen;
	uncompress(reinterpret_cast<Bytef*>(out), &decompressedSize, reinterpret_cast<const Bytef*>(data), inLen);
}
}

#endif

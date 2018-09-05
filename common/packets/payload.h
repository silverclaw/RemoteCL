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

#if !defined(REMOTECL_PACKET_PAYLOAD_H)
#define REMOTECL_PACKET_PAYLOAD_H
/// @file payload.h
/// Defines packets to transfer generic data block.
/// If compression is enabled, the bursts will be automatically (de)compressed
/// if the size is above the threshold.

#include <vector>

#if defined(REMOTECL_USE_ZLIB)
#include "compression.h"
#endif
#include "packet.h"

namespace RemoteCL
{
/// If a SizeT template parameter for the payloads isn't specified, this is the default type.
using PayloadDefaultSizeT = uint32_t;

/// Transfers a burst of generic data across the socket.
/// @tparam SizeT The type used to encode the size of the burst.
/// For small payloads, a smaller size can be selected to reduce wire overhead.
template<typename SizeT = PayloadDefaultSizeT>
struct Payload : public Packet
{
	enum : PayloadDefaultSizeT {
		/// The threshold above which the payload will be compressed.
		// Defaults 1MB - picked arbitrarily.
		CompressionSizeThreshold = 1 << 20
	};

	Payload() noexcept : Packet(PacketType::Payload) {}

	std::vector<uint8_t> mData;
};

/// Used to describe a payload where the data being sent isn't copied or owned by the packet.
template<typename SizeT = PayloadDefaultSizeT>
struct PayloadPtr : public Packet
{
	PayloadPtr() noexcept : Packet(PacketType::Payload) {}
	PayloadPtr(const void* ptr, std::size_t size) noexcept : Packet(PacketType::Payload), mPtr(ptr), mSize(size) {}
	template<typename T>
	PayloadPtr(const std::vector<T>& data) noexcept : PayloadPtr(data.data(), data.size()*sizeof(T)) {}

	const void* mPtr = nullptr;
	SizeT mSize = 0;
};

/// De-serialise a payload packet directly into this pointer.
template<typename SizeT = PayloadDefaultSizeT>
struct PayloadInto : public Packet
{
	PayloadInto() = delete;
	PayloadInto(void* ptr) noexcept : Packet(PacketType::Payload), mPtr(ptr) {}

	void* mPtr = nullptr;
};

// PayloadPtr can only be serialised
template<typename SizeT>
SocketStream& operator <<(SocketStream& o, const PayloadPtr<SizeT>& p)
{
#if defined(REMOTECL_USE_ZLIB)
	if (p.mSize >= Payload<>::CompressionSizeThreshold) {
		// Attempt to compress the data.
		std::vector<uint8_t> compressed = Compress(p.mPtr, p.mSize);
		if (compressed.size() != 0 && compressed.size() < p.mSize) {
			// Send decompressed size first.
			o << p.mSize;
			// Raw data buffer follows
			SizeT compressedSize = compressed.size();
			o << compressedSize;
			o.write(compressed.data(), compressed.size());
			return o;
		}
	}

	// No compression.
	SizeT zero = 0;
	o << zero;
#endif

	o << p.mSize;
	if (p.mSize) o.write(p.mPtr, p.mSize);
	return o;
}

// PayloadInto can only be de-serialised.
template<typename SizeT>
SocketStream& operator >>(SocketStream& i, PayloadInto<SizeT>& p)
{
#if defined(REMOTECL_USE_ZLIB)
	SizeT decompressedSize;
	i >> decompressedSize;
#endif
	SizeT dataSize;
	i >> dataSize;

#if defined(REMOTECL_USE_ZLIB)
	if (decompressedSize != 0) {
		// Read the compressed data.
		std::vector<uint8_t> compressed;
		compressed.resize(dataSize);
		i.read(compressed.data(), dataSize);
		// decompress into the output buffer
		Decompress(compressed.data(), compressed.size(), p.mPtr, decompressedSize);
		return i;
	}
#endif

	if (dataSize) i.read(p.mPtr, dataSize);
	return i;
}

template<typename SizeT>
SocketStream& operator <<(SocketStream& o, const Payload<SizeT>& p)
{
#if defined(REMOTECL_USE_ZLIB)
	// Should we try to compress the payload?
	if (p.mData.size() >= Payload<>::CompressionSizeThreshold) {
		std::vector<uint8_t> compressed = Compress(p.mData.data(), p.mData.size());
		if (compressed.size() != 0 && compressed.size() < p.mData.size()) {
			SizeT decompressedSize = p.mData.size();
			SizeT compressedSize  = compressed.size();
			o << decompressedSize << compressedSize;
			o.write(compressed.data(), compressed.size());
			return o;
		}
	}
	SizeT decompressedSize = 0;
	o << decompressedSize;
#endif

	SizeT dataSize = p.mData.size();
	o << dataSize;
	o.write(p.mData.data(), dataSize);
	return o;
}

template<typename SizeT>
SocketStream& operator >>(SocketStream& i, Payload<SizeT>& p)
{
#if defined(REMOTECL_USE_ZLIB)
	SizeT decompressedSize;
	i >> decompressedSize;
#endif

	SizeT dataSize;
	i >> dataSize;
	p.mData.resize(dataSize);
	i.read(p.mData.data(), dataSize);

#if defined(REMOTECL_USE_ZLIB)
	// Undo compression if any.
	if (decompressedSize != 0) {
		std::vector<uint8_t> decompressed;
		decompressed.resize(decompressedSize);
		Decompress(p.mData.data(), p.mData.size(), decompressed.data(), decompressed.size());
		p.mData.swap(decompressed);
	}
#endif
	return i;
}
}

#endif

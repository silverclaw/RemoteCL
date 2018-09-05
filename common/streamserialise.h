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

#if !defined(REMOTECL_SOCKETSTREAMSERIALISE_H)
#define REMOTECL_SOCKETSTREAMSERIALISE_H
/// @file streamserialise.h
/// Defines a packet to transfer an STL container.
/// Also adds functions to transfer a std::string and std::array.

#include "socketstream.h"

#include <string>
#include <limits>
#include <array>

namespace RemoteCL
{
/// Wraps a container on a serialiseable interface.
/// @tparam Container The container to transfer.
/// @tparam SizeT The type used to encode the size of the container.
template<typename Container, typename SizeT = uint16_t>
struct Serialiseable : public Container
{
	// Empty on purpose.
};

template<typename Container, typename SizeT>
SocketStream& operator<<(SocketStream& s, const Serialiseable<Container, SizeT>& C)
{
	assert(C.size() <= std::numeric_limits<SizeT>::max());
	SizeT size = C.size();
	s << size;
	for (const typename Container::value_type& e : C) s << e;
	return s;
}

template<typename Container, typename SizeT>
SocketStream& operator>>(SocketStream& s, Serialiseable<Container, SizeT>& C)
{
	SizeT size = 0;
	s >> size;
	C.reserve(C.size() + size);
	for (SizeT i = 0; i < size; ++i) {
		typename Container::value_type e;
		s >> e;
		C.insert(C.end(), e);
	}
	return s;
}

inline SocketStream& operator <<(SocketStream& o, const std::string& s)
{
	assert(s.length() <= std::numeric_limits<uint16_t>::max());
	o << static_cast<uint16_t>(s.length());
	o.write(s.data(), s.length());
	return o;
}

inline SocketStream& operator >>(SocketStream& i, std::string& s)
{
	uint16_t length;
	i >> length;
	s.reserve(length);
	// TODO: Rewrite.
	while (length != 0) {
		char c;
		i >> c;
		s += c;
		--length;
	}

	return i;
}

template<typename T, std::size_t N>
SocketStream& operator <<(SocketStream& o,  const std::array<T, N>& a)
{
	for (const T& t : a) o << t;
	return o;
}

template<typename T, std::size_t N>
SocketStream& operator >>(SocketStream& i, std::array<T, N>& a)
{
	for (T& t : a) i >> t;
	return i;
}

}

#endif

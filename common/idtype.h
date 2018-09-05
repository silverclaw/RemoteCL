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

#if !defined(REMOTECL_ID_TYPE_H)
#define REMOTECL_ID_TYPE_H
/// @file idtype.h Defines the type used to describe an OpenCL object ID.

#include <cstdint>

namespace RemoteCL
{
/// The underlying integer type used for OpenCL Object IDs, used to translate
/// the object allocation from client to server.
/// A small integer type is more efficient to transfer on slow connections, but
/// obviously limits the number of objects that can be allocated.
using IDType = uint16_t;
}

#endif

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

#if !defined(REMOTECL_CLIENT_MEMMAPPING_H)
#define REMOTECL_CLIENT_MEMMAPPING_H

#include "CL/cl.h"

#include "idtype.h"

#include <memory>

namespace RemoteCL
{
namespace Client
{
struct CLMappedBuffer
{
    explicit CLMappedBuffer(IDType id) noexcept : ID(id) {}

    const IDType ID;

    std::unique_ptr<uint8_t[]> data;
    cl_map_flags flags;
    size_t offset;
    size_t size;
};

}
}

#endif

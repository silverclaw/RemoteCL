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

#if !defined(REMOTECL_CLIENT_API_UTIL)
#define REMOTECL_CLIENT_API_UTIL
/// @file apiutil.h Defines macros and functions for implementation simplicity.

#include <cstdint>

namespace RemoteCL
{
namespace Client
{
/// Used for API query functions where the return data goes into a buffer.
/// Does a store of this primitive if sufficient space exists.
template<typename T>
void Store(T data, void* ptr, std::size_t availableSize, std::size_t* sizeRet)
{
	if (availableSize >= sizeof(T)) {
		*(reinterpret_cast<T*>(ptr)) = data;
	}
	if (sizeRet != nullptr) {
		*sizeRet = sizeof(T);
	}
}
} // namespace Client
} // namespace RemoteCL

// Injects an error return into the program where the code (X)
// is stored out in the errcode_ret variable.
#define ReturnError(X) \
	do { \
		if (errcode_ret != nullptr) *errcode_ret = X; \
		return nullptr; \
	} while(false);

#endif

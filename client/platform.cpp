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

#include <cstdint>
#include <cstring>

#include "CL/cl_platform.h"
#include "hints.h"
#include "connection.h"
#include "objects.h"
#include "packets/platform.h"
#include "packets/IDs.h"
#include "packets/payload.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) CL_API_SUFFIX__VERSION_1_0
{
	if (platforms == nullptr && num_platforms == nullptr) return CL_INVALID_VALUE;
	if (platforms != nullptr && num_entries == 0) return CL_INVALID_VALUE;

	if (num_platforms) *num_platforms = 0;

	try {
		auto conn = gConnection.get();
		// Request the platform list...
		conn->write<GetPlatformIDs>({});
		conn->flush();
		// ... and wait for it to arrive.
		IDListPacket list = conn->read<IDListPacket>();

		// Create a platform for each ID queried.
		if (num_platforms) *num_platforms = list.mIDs.size();
		uint32_t entry = 0;
		for (auto& p : list.mIDs) {
			PlatformID& platform = conn.getOrInsertObject<PlatformID>(p);
			if (entry < num_entries) {
				platforms[entry] = platform;
				entry++;
			}
		}
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (const std::bad_alloc&) {
		return CL_OUT_OF_HOST_MEMORY;
	} catch (...) {
		// Do nothing - the socket probably closed.
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name,
                  size_t param_value_size, void* param_value,
                  size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (platform == nullptr) return CL_INVALID_PLATFORM;

	try {
		IDType id = GetID(platform);
		auto conn = gConnection.get();
		conn->write<GetPlatformInfo>({id, param_name});
		conn->flush();

		// All platform queries can be passed down straight to client.
		Payload<uint8_t> payload = conn->read<Payload<uint8_t>>();
		if (param_value_size_ret) *param_value_size_ret = payload.mData.size();
		if (param_value && param_value_size >= payload.mData.size()) {
			std::memcpy(param_value, payload.mData.data(), payload.mData.size());
		}

		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

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
#include <iostream>

#include "CL/cl_platform.h"
#include <cstring>
#include "hints.h"
#include "apiutil.h"
#include "connection.h"
#include "objects.h"
#include "packets/platform.h"
#include "packets/IDs.h"
#include "packets/refcount.h"
#include "packets/device.h"
#include "packets/payload.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;


SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_0
{
	try {
		auto conn = gConnection.get();
		conn->write<Retain>({'D', GetID(device)});
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_0
{
	try {
		auto conn = gConnection.get();
		conn->write( Release('D', GetID(device)));
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type,
               cl_uint num_entries, cl_device_id* devices,
               cl_uint* num_devices) CL_API_SUFFIX__VERSION_1_0
{
	if (devices != nullptr && num_entries == 0) return CL_INVALID_VALUE;
	if (devices == nullptr && num_devices == nullptr) return CL_INVALID_VALUE;

	if (num_devices) *num_devices = 0;

	try {
		auto conn = gConnection.get();
		IDType platID = 0;
		if (platform) platID = GetID(platform);

		// Request the device list...
		conn->write(GetDeviceIDs(platID, device_type));
		conn->flush();
		// ... and wait for it to arrive.
		IDListPacket list = conn->read<IDListPacket>();

		// Create a device for each ID queried.
		if (num_devices) *num_devices = list.mIDs.size();
		uint32_t entry = 0;
		for (auto& p : list.mIDs) {
			DeviceID& device = conn.getOrInsertObject<DeviceID>(p);
			if (entry < num_entries) {
				devices[entry] = device;
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
clGetDeviceInfo(cl_device_id device, cl_device_info param_name,
                size_t param_value_size, void* param_value,
                size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (device == nullptr) return CL_INVALID_DEVICE;

	try {
		auto conn = gConnection.get();

		conn->write<GetDeviceInfo>({GetID(device), param_name});
		conn->flush();

		switch (param_name) {
			// These queries must be ID-translated.
			case CL_DEVICE_PLATFORM: {
				PlatformID& id = conn.getOrInsertObject<PlatformID>(conn->read<IDPacket>());
				Store<cl_platform_id>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}
			case CL_DEVICE_PARENT_DEVICE: {
				DeviceID& id = conn.getOrInsertObject<DeviceID>(conn->read<IDPacket>());
				Store<cl_device_id>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}

			// These queries must be std::size_t translated.
			case CL_DEVICE_PRINTF_BUFFER_SIZE:
			case CL_DEVICE_IMAGE2D_MAX_WIDTH:
			case CL_DEVICE_IMAGE3D_MAX_WIDTH:
			case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
			case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
			case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
			case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
			case CL_DEVICE_IMAGE3D_MAX_DEPTH:
			case CL_DEVICE_IMAGE_PITCH_ALIGNMENT:
			case CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT:
			case CL_DEVICE_PROFILING_TIMER_RESOLUTION:
			case CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE:
			case CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE:
			case CL_DEVICE_MAX_PARAMETER_SIZE:
			case CL_DEVICE_MAX_WORK_GROUP_SIZE: {
				uint64_t value = conn->read<SimplePacket<PacketType::Payload, uint64_t>>();
				Store<uint64_t>(value, param_value, param_value_size, param_value_size_ret);
				break;
			}
			case CL_DEVICE_MAX_WORK_ITEM_SIZES: {
				uint64_t sizes[3];
				conn->read(PayloadInto<uint8_t>(sizes));
				std::size_t* retVal = reinterpret_cast<std::size_t*>(param_value);
				if (param_value_size_ret) *param_value_size_ret = 3 * sizeof(std::size_t);
				if (retVal && param_value_size >= 3 * sizeof(std::size_t)) {
					retVal[0] = sizes[0];
					retVal[1] = sizes[1];
					retVal[2] = sizes[2];
				}
				break;
			}

			// All others, just memcpy whatever the server returned.
			default: {
				Payload<> payload = conn->read<Payload<>>();
				if (param_value_size_ret) *param_value_size_ret = payload.mData.size();
				if (param_value && param_value_size >= payload.mData.size()) {
					std::memcpy(param_value, payload.mData.data(), payload.mData.size());
				}
				break;
			}
		}
		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (const std::bad_alloc&) {
		return CL_OUT_OF_HOST_MEMORY;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

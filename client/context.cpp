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
#include "packets/IDs.h"
#include "packets/refcount.h"
#include "packets/context.h"
#include "packets/payload.h"
#include "objects.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

#define ReturnError(X) \
	do { \
	if (errcode_ret != nullptr) *errcode_ret = X; \
	return nullptr; \
	} while(false);

SO_EXPORT CL_API_ENTRY cl_context CL_API_CALL
clCreateContext(const cl_context_properties* properties,
                cl_uint num_devices, const cl_device_id* devices,
                void (CL_CALLBACK* pfn_notify)(const char *, const void *, size_t, void *),
                void* user_data,  cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	// We don't currently support callbacks. We can support them using the OOB stream,
	// but that's not implemented yet.
	if (pfn_notify != nullptr || user_data != nullptr) {
		std::cerr << "Callback functions not supported (yet)";
		//ReturnError(CL_INVALID_OPERATION)
	}

	if (num_devices == 0) ReturnError(CL_INVALID_PLATFORM);
	if (devices == nullptr) ReturnError(CL_INVALID_PLATFORM);

	try {
		CreateContext packet;
		// Properties come in pairs.
		if (properties != nullptr) {
			while (properties[0] != 0) {
				packet.mProperties.push_back(properties[0]);
				if (properties[0] == CL_CONTEXT_PLATFORM) {
					cl_platform_id id = reinterpret_cast<cl_platform_id>(properties[1]);
					packet.mProperties.push_back(GetID(id));
				} else {
					packet.mProperties.push_back(properties[1]);
				}
				properties++;
				properties++;
			}
		}

		packet.mDevices.reserve(num_devices);
		for (unsigned device = 0; device < num_devices; ++device) {
			if (devices[device] == nullptr) {
				ReturnError(CL_INVALID_DEVICE);
			}
			packet.mDevices.push_back(GetID(devices[device]));
		}

		LockedStream stream = gConnection.getLockedStream();
		if (!stream) ReturnError(CL_DEVICE_NOT_AVAILABLE);

		stream->write(packet);
		stream->flush();
		// We expect a single ID.
		IDPacket ID = stream->read<IDPacket>();
		Context& C = gConnection.registerID<Context>(ID);
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return C;
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE)
		return nullptr;
	}
}

SO_EXPORT CL_API_ENTRY cl_context CL_API_CALL
clCreateContextFromType(const cl_context_properties* properties, cl_device_type device_type,
                        void (CL_CALLBACK* pfn_notify)(const char *, const void *, size_t, void *),
                        void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	// We don't currently support callbacks. We can support them using the OOB stream,
	// but that's not implemented yet.
	if (pfn_notify != nullptr || user_data != nullptr) {
		std::cerr << "Callback functions not supported (yet)";
		if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
		return nullptr;
	}

	try {
		CreateContextFromType packet;
		packet.mDeviceType = device_type;
		// Properties come in pairs.
		if (properties != nullptr) {
			while (properties[0] != 0) {
				packet.mProperties.push_back(properties[0]);
				if (properties[0] == CL_CONTEXT_PLATFORM) {
					cl_platform_id id = reinterpret_cast<cl_platform_id>(properties[1]);
					packet.mProperties.push_back(GetID(id));
				} else {
					packet.mProperties.push_back(properties[1]);
				}
				properties++;
				properties++;
			}
		}

		LockedStream stream = gConnection.getLockedStream();
		if (!stream) ReturnError(CL_DEVICE_NOT_AVAILABLE);
		stream->write(packet);
		stream->flush();
		// We expect a single ID.
		IDPacket ID = stream->read<IDPacket>();
		Context& C = gConnection.registerID<Context>(ID);
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return C;
	} catch (...) {
		if (errcode_ret) *errcode_ret = CL_DEVICE_NOT_AVAILABLE;
		return nullptr;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetContextInfo(cl_context context, cl_context_info param_name,
                 size_t param_value_size, void* param_value,
                 size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) return CL_INVALID_CONTEXT;

	LockedStream stream = gConnection.getLockedStream();
	if (!stream) return CL_DEVICE_NOT_AVAILABLE;
	IDType id = GetID(context);

	try {
		stream->write<GetContextInfo>({id, param_name});
		stream->flush();
		Payload<uint8_t> payload = stream->read<Payload<uint8_t>>();
		if (param_value_size_ret) {
			if (param_name == CL_CONTEXT_DEVICES) {
				assert((payload.mData.size() % sizeof(IDType)) == 0);
				*param_value_size_ret = (payload.mData.size() / sizeof(IDType)) * sizeof(cl_device_id);
			} else {
				*param_value_size_ret = payload.mData.size();
			}
		}

		if (param_value) {
			if (param_name == CL_CONTEXT_DEVICES) {
				// TODO: This might be problematic when the std::vector data isn't IDType aligned.
				const IDType* ids = reinterpret_cast<IDType*>(payload.mData.data());
				assert((payload.mData.size() % sizeof(IDType)) == 0);
				/// Number of IDs returned by the server.
				const auto idCount = payload.mData.size() / sizeof(IDType);
				/// The return data, which will contain device IDs.
				cl_device_id* ret = static_cast<cl_device_id*>(param_value);
				/// The number of IDs that can be returned through param_value.
				auto available = std::min(idCount, param_value_size / sizeof(cl_device_id));
				while (available != 0) {
					*ret = gConnection.getOrInsertObject<DeviceID>(*ids);
					++ret;
					++ids;
					--available;
				}
			} else {
				std::memcpy(param_value, payload.mData.data(), std::min(param_value_size, payload.mData.size()));
			}
		}
		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) return CL_INVALID_CONTEXT;
	LockedStream stream = gConnection.getLockedStream();
	if (!stream) return CL_DEVICE_NOT_AVAILABLE;

	try {
		IDType id = GetID(context);
		stream->write<Retain>({'C', id});
		stream->flush();
		stream->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) return CL_INVALID_CONTEXT;
	LockedStream stream = gConnection.getLockedStream();
	if (!stream) return CL_DEVICE_NOT_AVAILABLE;

	try {
		IDType id = GetID(context);
		stream->write<Release>({'C', id});
		stream->flush();
		stream->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetSupportedImageFormats(cl_context context, cl_mem_flags flags,
                           cl_mem_object_type image_type, cl_uint num_entries,
                           cl_image_format* image_formats, cl_uint* num_image_formats) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) return CL_INVALID_CONTEXT;

	try {
		LockedStream stream = gConnection.getLockedStream();
		if (!stream) return CL_DEVICE_NOT_AVAILABLE;

		GetImageFormats query;
		query.mContextID = GetID(context);
		query.mFlags = flags;
		query.mImageType = image_type;
		stream->write(query);
		stream->flush();
		// The server will reply with a payload
		Payload<uint16_t> payload = stream->read<Payload<uint16_t>>();
		if (num_image_formats) {
			*num_image_formats = payload.mData.size() / sizeof(cl_image_format);
		}
		if (image_formats) {
			std::memcpy(image_formats, payload.mData.data(),
			            std::min(num_entries*sizeof(cl_image_format), payload.mData.size()));
		}
		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

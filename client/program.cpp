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

#include "objects.h"

#include <string>
#include <cstring>
#include <sstream>

#include "hints.h"
#include "connection.h"
#include "packets/refcount.h"
#include "packets/program.h"
#include "packets/IDs.h"
#include "packets/payload.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

#define ReturnError(X) \
	do { \
	if (errcode_ret != nullptr) *errcode_ret = X; \
	return nullptr; \
	} while(false);

SO_EXPORT CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithSource(cl_context context,
                          cl_uint count, const char** strings, const size_t* lengths,
                          cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (count == 0 || strings == nullptr) ReturnError(CL_INVALID_CONTEXT);

	try {
		std::ostringstream srcStream;

		// Copy the program source to a single buffer.
		for (cl_uint line = 0; line < count; ++line) {
			if (strings[line] == nullptr) ReturnError(CL_INVALID_VALUE);
			std::size_t strLen = 0;
			if (lengths != nullptr) strLen = lengths[line];
			// TODO: Not sure if this is correct - should we trust the host app
			// to never specify a 0? Should we really check the string for the length instead?
			if (strLen == 0) strLen = std::strlen(strings[line]);
			srcStream.write(strings[line], strLen);
		}

		srcStream.flush();
		ProgramSource packet;
		packet.mString = srcStream.str();

		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream, CL_DEVICE_NOT_AVAILABLE);
		packet.mID = GetID(context);
		stream.write(packet);
		stream.flush();
		IDPacket ID = stream.read<IDPacket>();
		Program& P = gConnection.registerID<Program>(ID);
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return P;
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (const std::bad_alloc&) {
		ReturnError(CL_OUT_OF_HOST_MEMORY);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithBinary(cl_context context, cl_uint num_devices,
                          const cl_device_id* device_list,
                          const size_t* lengths, const unsigned char** binaries,
                          cl_int* binary_status, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (num_devices == 0 || device_list == nullptr) ReturnError(CL_INVALID_VALUE);
	if (lengths == nullptr || binaries == nullptr) ReturnError(CL_INVALID_VALUE);

	try {
		IDListPacket deviceList;
		for (unsigned dev = 0; dev < num_devices; ++dev) {
			if (device_list[dev] == nullptr) {
				ReturnError(CL_INVALID_DEVICE);
			}
			deviceList.mIDs.push_back(GetID(device_list[dev]));
		}

		GetStreamErrRet(stream, CL_DEVICE_NOT_AVAILABLE);
		stream.write<BinaryProgram>({GetID(context)});
		stream.write(deviceList);
		for (unsigned bin = 0; bin < num_devices; ++bin) {
			stream.write<PayloadPtr<uint32_t>>({binaries[bin], binaries[bin] ? lengths[bin] : 0});
		}

		stream.flush();
		Program& P = gConnection.registerID<Program>(stream.read<IDPacket>());

		if (binary_status) {
			// Stream into results.
			stream.read<PayloadInto<uint16_t>>(binary_status);
		} else {
			// Discard the packet.
			stream.read<Payload<uint16_t>>();
		}

		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return P;
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (const std::bad_alloc&) {
		ReturnError(CL_OUT_OF_HOST_MEMORY);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
               const char* options, void (CL_CALLBACK* pfn_notify)(cl_program, void*),
               void* user_data) CL_API_SUFFIX__VERSION_1_0
{
	if (program == nullptr) return CL_INVALID_PROGRAM;
	if (device_list == nullptr && num_devices > 0) return CL_INVALID_VALUE;
	if (device_list != nullptr && num_devices == 0) return CL_INVALID_VALUE;
	if (pfn_notify == nullptr && user_data != nullptr) return CL_INVALID_VALUE;
	for (unsigned dev = 0; dev < num_devices; ++dev) {
		if (device_list[dev] == nullptr) return CL_INVALID_DEVICE;
	}

	try {
		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_DEVICE_NOT_AVAILABLE);

		BuildProgram build;
		build.mID = GetID(program);
		if (options) build.mString = options;

		IDListPacket ids;
		ids.mIDs.reserve(num_devices);
		for (unsigned d = 0; d < num_devices; ++d) ids.mIDs.push_back(GetID(device_list[d]));

		stream.write(build);
		stream.write(ids);
		stream.flush();
		stream.read<SuccessPacket>();
		return CL_SUCCESS;
	} catch (const std::bad_alloc&) {
		return CL_OUT_OF_HOST_MEMORY;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetProgramBuildInfo(cl_program program, cl_device_id device, cl_program_build_info param_name,
                      size_t param_value_size, void* param_value,
                      size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (program == nullptr) return CL_INVALID_PROGRAM;
	if (device == nullptr) return CL_INVALID_DEVICE;

	try {
		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_DEVICE_NOT_AVAILABLE);

		ProgramBuildInfo info;
		info.mParam = param_name;
		info.mProgramID = GetID(program);
		info.mDeviceID = GetID(device);
		stream.write(info);
		stream.flush();
		Payload<> payload = stream.read<Payload<>>();
		if (param_value_size_ret) {
			*param_value_size_ret = payload.mData.size();
		}
		if (param_value) {
			std::memcpy(param_value, payload.mData.data(), std::min(param_value_size, payload.mData.size()));
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

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetProgramInfo(cl_program program, cl_program_info param_name,
                 size_t param_value_size, void* param_value,
                 size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (program == nullptr) return CL_INVALID_PROGRAM;

	try {
		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_DEVICE_NOT_AVAILABLE);

		stream.write<ProgramInfo>({GetID(program), param_name});
		stream.flush();

		if (param_name == CL_PROGRAM_CONTEXT) {
			IDPacket id = stream.read<IDPacket>();
			// We know the context exists.
			Context* context = gConnection.getObject<Context>(id);
			assert(context != nullptr);
			if (param_value_size_ret) *param_value_size_ret = sizeof(cl_context);
			if (param_value && param_value_size >= sizeof(cl_context)) {
				*reinterpret_cast<cl_context*>(param_value) = *context;
			}
		} else if (param_name == CL_PROGRAM_DEVICES) {
			IDListPacket list = stream.read<IDListPacket>();
			if (param_value_size_ret) *param_value_size_ret = list.mIDs.size() * sizeof(cl_device_id);
			if (param_value && param_value_size >= list.mIDs.size() * sizeof(cl_device_id)) {
				cl_device_id* ids = reinterpret_cast<cl_device_id*>(param_value);
				for (IDType id : list.mIDs) {
					*ids = *gConnection.getObject<DeviceID>(id);
					++ids;
				}
			}
		} else if (param_name == CL_PROGRAM_BINARIES) {
			// Get how many binaries are there.
			const uint8_t binaryCount = stream.read<SimplePacket<PacketType::Payload, uint8_t>>();
			// Receive all binaries.
			std::vector<Payload<uint16_t>> binaries;
			binaries.reserve(binaryCount);
			for (std::size_t i = 0; i < binaryCount; ++i) {
				binaries.emplace_back(stream.read<Payload<uint16_t>>());
			}

			// Now that all data has been received, pass it down to the application.
			if (param_value_size_ret) {
				*param_value_size_ret = binaryCount * sizeof(char*);
			}

			// This can be done more efficiently through read<PayloadInto> but it gets
			// complicated if we need to stop or skip a binary.
			if (char** ptrs = reinterpret_cast<char**>(param_value)) {
				for (std::size_t i = 0; i < binaryCount; ++i) {
					if (ptrs[i]) {
						std::memcpy(ptrs[i], binaries[i].mData.data(), binaries[i].mData.size());
					}
				}
			}
		} else {
			Payload<> payload = stream.read<Payload<>>();
			if (param_value_size_ret) {
				*param_value_size_ret = payload.mData.size();
			}
			if (param_value) {
				std::memcpy(param_value, payload.mData.data(), std::min(param_value_size, payload.mData.size()));
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

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainProgram(cl_program program) CL_API_SUFFIX__VERSION_1_0
{
	if (program == nullptr) return CL_INVALID_PROGRAM;

	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<Retain>({'P', GetID(program)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseProgram(cl_program program) CL_API_SUFFIX__VERSION_1_0
{
	if (program == nullptr) return CL_INVALID_PROGRAM;

	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<Release>({'P', GetID(program)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

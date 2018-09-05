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
#include "hints.h"
#include "connection.h"
#include "objects.h"
#include "packets/refcount.h"
#include "packets/queue.h"
#include "packets/IDs.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

#define ReturnError(X) \
	do { \
	if (errcode_ret != nullptr) *errcode_ret = X; \
	return nullptr; \
	} while(false);


SO_EXPORT CL_API_ENTRY cl_command_queue CL_API_CALL
clCreateCommandQueue(cl_context context, cl_device_id device,
                     cl_command_queue_properties properties,
                     cl_int* errcode_ret)
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (device == nullptr) ReturnError(CL_INVALID_DEVICE);

	try {
		CreateQueue packet;
		packet.mContext = GetID(context);
		packet.mDevice = GetID(device);
		packet.mProp = properties;

		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream, CL_SUCCESS);
		stream.write(packet);
		stream.flush();
		// We expect a single ID.
		IDPacket ID = stream.read<IDPacket>();
		Queue& Q = gConnection.registerID<Queue>(ID);
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return Q;
	} catch (const std::bad_alloc&) {
		ReturnError(CL_OUT_OF_HOST_MEMORY);
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

#if (CL_TARGET_OPENCL_VERSION >= 200)
SO_EXPORT CL_API_ENTRY cl_command_queue CL_API_CALL
clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties,
                                   cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (device == nullptr) ReturnError(CL_INVALID_DEVICE);
	auto contextLock(gConnection.getLock());
	GetStreamErrRet(stream, CL_SUCCESS);

	try {
		CreateQueueWithProp packet;
		packet.mContext = GetID(context);
		packet.mDevice = GetID(device);
		if (properties) {
			while (*properties != 0) {
				// Copy the property ID and payload.
				packet.mProperties.push_back(*properties);
				++properties;
				packet.mProperties.push_back(*properties);
				++properties;
			}
		}
		stream.write(packet);
		stream.flush();
		// We expect a single ID.
		IDPacket ID = stream.read<IDPacket>();
		Queue& Q = gConnection.registerID<Queue>(ID);
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return Q;
	} catch (const std::bad_alloc&) {
		ReturnError(CL_OUT_OF_HOST_MEMORY);
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}
#endif

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clFlush(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<QFlushPacket>(GetID(command_queue));
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clFinish(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<QFinishPacket>(GetID(command_queue));
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<Retain>({'Q', GetID(command_queue)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	auto contextLock(gConnection.getLock());
	GetStream(stream, CL_SUCCESS);

	try {
		stream.write<Release>({'Q', GetID(command_queue)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

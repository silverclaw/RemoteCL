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
#include <cstring>

#include "CL/cl_platform.h"
#include "hints.h"
#include "connection.h"
#include "objects.h"
#include "apiutil.h"
#include "packets/refcount.h"
#include "packets/queue.h"
#include "packets/IDs.h"
#include "packets/payload.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;


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

		auto conn = gConnection.get();
		conn->write(packet);
		conn->flush();
		// We expect a single ID.
		IDPacket ID = conn->read<IDPacket>();
		Queue& Q = conn.registerID<Queue>(ID);
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

SO_EXPORT CL_API_ENTRY cl_command_queue CL_API_CALL
clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties,
                                   cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (device == nullptr) ReturnError(CL_INVALID_DEVICE);

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
		auto conn = gConnection.get();
		conn->write(packet);
		conn->flush();
		// We expect a single ID.
		IDPacket ID = conn->read<IDPacket>();
		Queue& Q = conn.registerID<Queue>(ID);
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

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetCommandQueueInfo(cl_command_queue command_queue, cl_command_queue_info param_name,
                      size_t param_value_size, void* param_value,
                      size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;

	try {
		auto queue = GetID(command_queue);
		IDParamPair<PacketType::GetQueueInfo> query;
		query.mID = queue;
		query.mData = param_name;

		auto conn = gConnection.get();

		conn->write(query).flush();

		switch (param_name) {
			// These queries must be ID-translated.
			case CL_QUEUE_CONTEXT: {
				Context& id = conn.getOrInsertObject<Context>(conn->read<IDPacket>());
				Store<cl_context>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}
			case CL_QUEUE_DEVICE: {
				DeviceID& id = conn.getOrInsertObject<DeviceID>(conn->read<IDPacket>());
				Store<cl_device_id>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}

			// All others, just memcpy whatever the server returned.
			default: {
				// The only other queries are queue size and queue properties,
				// which should fit in 4 bytes.
				Payload<uint8_t> payload = conn->read<Payload<uint8_t>>();
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

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clFlush(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;

	try {
		auto conn = gConnection.get();
		conn->write<QFlushPacket>(GetID(command_queue));
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
clFinish(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;

	try {
		auto conn = gConnection.get();
		conn->write<QFinishPacket>(GetID(command_queue));
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
clRetainCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;

	try {
		auto conn = gConnection.get();
		conn->write<Retain>({'Q', GetID(command_queue)});
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
clReleaseCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;

	try {
		auto conn = gConnection.get();
		conn->write<Release>({'Q', GetID(command_queue)});
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}

	return CL_SUCCESS;
}

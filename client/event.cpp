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

#include "hints.h"
#include "connection.h"
#include "apiutil.h"
#include "packets/refcount.h"
#include "packets/IDs.h"
#include "packets/commands.h"
#include "packets/event.h"
#include "packets/simple.h"

#include <cstring>

using namespace RemoteCL;
using namespace RemoteCL::Client;


SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel,
                       cl_uint work_dim, const size_t* global_work_offset,
                       const size_t* global_work_size, const size_t* local_work_size,
                       cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                       cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (kernel == nullptr) return CL_INVALID_KERNEL;
	if (work_dim == 0 || work_dim > 3) return CL_INVALID_VALUE;

	try {
		EnqueueKernel E;
		E.mKernelID = GetID(kernel);
		E.mQueueID = GetID(command_queue);
		E.mWorkDim = work_dim;
		for (unsigned i = 0; i < work_dim; ++i) {
			E.mGlobalSize[i] = global_work_size[i];
			if (global_work_offset) E.mGlobalOffset[i] = global_work_offset[i];
			else E.mGlobalOffset[i] = 0;
			if (local_work_size) {
				// We only check validity of the local work size because
				// we use the '0' as a special value.
				if (local_work_size[i] == 0) return CL_INVALID_WORK_GROUP_SIZE;
				E.mLocalSize[i] = local_work_size[i];
			} else {
				E.mLocalSize[i] = 1;
			}
		}
		// Initialise the remaining dimensions on default values.
		// It won't matter as the server won't use them, but stops un-initialised memory warnings.
		for (unsigned  i = work_dim; i < 3; ++i) {
			E.mGlobalSize[i] = 1;
			E.mGlobalOffset[i] = 0;
			E.mLocalSize[i] = 1;
		}

		if (event) E.mWantEvent = true;
		IDListPacket eventList;
		if (num_events_in_wait_list) {
			if (!event_wait_list) return CL_INVALID_EVENT_WAIT_LIST;
			E.mExpectEventList = true;
			eventList.mIDs.reserve(num_events_in_wait_list);
			for (unsigned i = 0; i < num_events_in_wait_list; ++i) {
				if (!event_wait_list[i]) return CL_INVALID_EVENT;
				eventList.mIDs.push_back(GetID(event_wait_list[i]));
			}
		}

		auto conn = gConnection.get();

		conn->write(E);
		if (num_events_in_wait_list) {
			conn->write(eventList);
		}
		conn->flush();

		if (event) *event = conn.registerID<Event>(conn->read<IDPacket>());
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}
	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_event CL_API_CALL
clCreateUserEvent(cl_context context, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);

	try {
		auto conn = gConnection.get();
		conn->write<CreateUserEvent>({GetID(context)}).flush();
		cl_event ret = conn.registerID<Event>(conn->read<IDPacket>());
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return ret;
	} catch (const std::bad_alloc&) {
		ReturnError(CL_OUT_OF_HOST_MEMORY);
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clSetUserEventStatus(cl_event event, cl_int execution_status) CL_API_SUFFIX__VERSION_1_1
{
	if (event == nullptr) return CL_INVALID_EVENT;

	try {
		auto conn = gConnection.get();
		SetUserEventStatus packet;
		packet.mID = GetID(event);
		packet.mData = execution_status;
		conn->write(packet).flush();
		conn->read<SuccessPacket>();
		return CL_SUCCESS;
	} catch (const std::bad_alloc&) {
		return(CL_OUT_OF_HOST_MEMORY);
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetEventInfo(cl_event event, cl_event_info param_name,
               size_t  param_value_size, void* param_value,
               size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (event == nullptr) return CL_INVALID_EVENT;

	try {
		IDType id = GetID(event);
		auto conn = gConnection.get();
		conn->write<GetEventInfo>({id, param_name});
		conn->flush();

		switch (param_name) {
		case CL_EVENT_COMMAND_QUEUE: {
			auto ID = conn->read<IDPacket>();
			if (param_value_size_ret) *param_value_size_ret = sizeof(cl_command_queue);
			if (param_value && param_value_size >= sizeof(cl_command_queue)) {
				Queue* queue = conn.getObject<Queue>(ID.mData);
				cl_command_queue cmdQueue = *queue;
				std::memcpy(param_value, &cmdQueue, sizeof(cl_command_type));
			}
			break;
		}
		case CL_EVENT_CONTEXT:  {
			auto ID = conn->read<IDPacket>();
			if (param_value_size_ret) *param_value_size_ret = sizeof(cl_context);
			if (param_value && param_value_size >= sizeof(cl_context)) {
				Context* C = conn.getObject<Context>(ID.mData);
				cl_context context = *C;
				std::memcpy(param_value, &context, sizeof(cl_context));
			}
			break;
		}

		case CL_EVENT_COMMAND_TYPE: {
			auto payload = conn->read<SimplePacket<PacketType::Payload, uint32_t>>();
			if (param_value_size_ret) *param_value_size_ret = sizeof(cl_command_type);
			if (param_value && param_value_size >= sizeof(cl_command_type)) {
				cl_command_type type = static_cast<cl_command_type>(payload.mData);
				std::memcpy(param_value, &type, sizeof(cl_command_type));
			}
			break;
		}
		case CL_EVENT_COMMAND_EXECUTION_STATUS:
		case CL_EVENT_REFERENCE_COUNT:
		default:
			// The "default" should not trigger as the server would issue an error.
			// All queries will fit in 32bits (a cl_int).
			auto payload = conn->read<SimplePacket<PacketType::Payload, uint32_t>>();
			if (param_value_size_ret) *param_value_size_ret = sizeof(uint32_t);
			if (param_value && param_value_size >= sizeof(uint32_t)) {
				std::memcpy(param_value, &payload.mData, sizeof(uint32_t));
			}
			break;
		}

		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name,
                        size_t param_value_size, void* param_value,
                        size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (event == nullptr) return CL_INVALID_EVENT;

	try {
		IDType id = GetID(event);
		auto conn = gConnection.get();
		conn->write<GetEventProfilingInfo>({id, param_name});
		conn->flush();

		// All event queries will be 64bits (a cl_ulong).
		auto payload = conn->read<SimplePacket<PacketType::Payload, uint64_t>>();
		if (param_value_size_ret) *param_value_size_ret = sizeof(cl_ulong);
		if (param_value && param_value_size >= sizeof(cl_ulong)) {
			std::memcpy(param_value, &payload.mData, sizeof(cl_ulong));
		}

		return CL_SUCCESS;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clWaitForEvents(cl_uint num_events, const cl_event* event_list) CL_API_SUFFIX__VERSION_1_0
{
	if (num_events == 0) return CL_INVALID_VALUE;
	if (event_list == nullptr) return CL_INVALID_VALUE;

	try {
		IDListPacket eventList;
		eventList.mIDs.reserve(num_events);
		for (unsigned i = 0; i < num_events; ++i) {
			if (!event_list[i]) return CL_INVALID_EVENT;
			eventList.mIDs.push_back(GetID(event_list[i]));
		}

		auto conn = gConnection.get();

		conn->write<WaitForEvents>({});
		conn->write(eventList);
		conn->flush();
		conn->read<SuccessPacket>();
		return CL_SUCCESS;
	} catch (const std::bad_alloc&) {
		return CL_OUT_OF_HOST_MEMORY;
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainEvent(cl_event event) CL_API_SUFFIX__VERSION_1_0
{
	if (event == nullptr) return CL_INVALID_VALUE;

	try {
		auto conn = gConnection.get();
		conn->write<Retain>({'E', GetID(event)});
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}
	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseEvent(cl_event event) CL_API_SUFFIX__VERSION_1_0
{
	if (event == nullptr) return CL_INVALID_VALUE;

	try {
	auto conn = gConnection.get();
		conn->write<Release>({'E', GetID(event)});
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

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
#include "packetstream.h"
#include "packets/refcount.h"
#include "packets/IDs.h"
#include "packets/commands.h"
#include "packets/event.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

#define ReturnError(X) \
	do { \
	if (errcode_ret != nullptr) *errcode_ret = X; \
	return nullptr; \
	} while(false);

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel,
                       cl_uint work_dim, const size_t* global_work_offset,
                       const size_t* global_work_size, const size_t* local_work_size,
                       cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                       cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (kernel == nullptr) return CL_INVALID_KERNEL;
	if (work_dim >= 3) return CL_INVALID_VALUE;

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

		auto contextLock(gConnection.getLock());
		GetStream(stream);

		stream.write(E);
		if (num_events_in_wait_list) {
			stream.write(eventList);
		}
		stream.flush();

		if (event) *event = gConnection.registerID<Event>(stream.read<IDPacket>());
		stream.read<SuccessPacket>();
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
		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream);
		stream.write<CreateUserEvent>({GetID(context)}).flush();
		cl_event ret = gConnection.registerID<Event>(stream.read<IDPacket>());
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
		auto contextLock(gConnection.getLock());
		GetStream(stream);
		SetUserEventStatus packet;
		packet.mID = GetID(event);
		packet.mData = execution_status;
		stream.write(packet).flush();
		stream.read<SuccessPacket>();
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

		auto contextLock(gConnection.getLock());
		GetStream(stream);

		stream.write<WaitForEvents>({});
		stream.write(eventList);
		stream.flush();
		stream.read<SuccessPacket>();
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
		auto contextLock(gConnection.getLock());
		GetStream(stream);
		stream.write<Retain>({'E', GetID(event)});
		stream.flush();
		stream.read<SuccessPacket>();
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
		auto contextLock(gConnection.getLock());
		GetStream(stream);
		stream.write<Release>({'E', GetID(event)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}

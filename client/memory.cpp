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

#include <cstring>

#include "hints.h"
#include "connection.h"
#include "packetstream.h"
#include "packets/refcount.h"
#include "packets/memory.h"
#include "packets/IDs.h"
#include "packets/payload.h"
#include "packets/commands.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;

#define ReturnError(X) \
	do { \
	if (errcode_ret != nullptr) *errcode_ret = X; \
	return nullptr; \
	} while(false);

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clEnqueueFillBuffer(cl_command_queue command_queue, cl_mem buffer,
                    const void* pattern, size_t pattern_size,
                    size_t offset, size_t size,
                    cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                    cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (buffer == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		FillBuffer E;

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
		E.mBufferID = GetID(buffer);
		E.mSize = size;
		E.mOffset = offset;
		E.mQueueID = GetID(command_queue);
		E.mPatternSize = pattern_size;
		if (pattern_size > E.mPattern.size()) {
			return CL_INVALID_VALUE;
		}
		std::memcpy(E.mPattern.data(), pattern, pattern_size);

		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_SUCCESS);

		stream.write(E);
		if (num_events_in_wait_list) {
			stream.write(eventList);
		}
		stream.flush();

		if (event) *event = gConnection.registerID<Event>(stream.read<IDPacket>());
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
clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                    size_t offset, size_t size, void* ptr,
                    cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                    cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (buffer == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		ReadBuffer E;

		E.mBlock = blocking_read;

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

		E.mBufferID = GetID(buffer);
		E.mSize = size;
		E.mOffset = offset;
		E.mQueueID = GetID(command_queue);

		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_SUCCESS);

		stream.write(E);
		if (num_events_in_wait_list) {
			stream.write(eventList);
		}
		stream.flush();

		if (event) *event = gConnection.registerID<Event>(stream.read<IDPacket>());
		stream.read<PayloadInto<>>({ptr});
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
clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
                     size_t offset, size_t size, const void* ptr,
                     cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                     cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (buffer == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		WriteBuffer E;

		E.mBlock = blocking_write;

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

		E.mBufferID = GetID(buffer);
		E.mSize = size; // This is redundant because the payload will have this anyway.
		E.mOffset = offset;
		E.mQueueID = GetID(command_queue);

		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_SUCCESS);

		stream.write(E);
		if (num_events_in_wait_list) {
			stream.write(eventList);
		}
		stream.write<PayloadPtr<>>({ptr, size});
		stream.flush();

		if (event) *event = gConnection.registerID<Event>(stream.read<IDPacket>());
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

SO_EXPORT CL_API_ENTRY cl_mem CL_API_CALL
clCreateBuffer(cl_context context, cl_mem_flags flags, size_t size,
               void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	// We don't support host ptr yet.
	if ((flags & CL_MEM_USE_HOST_PTR) != 0) ReturnError(CL_INVALID_OPERATION);

	try {
		CreateBuffer packet;
		packet.mFlags = flags;
		packet.mSize = size;
		packet.mContextID = GetID(context);
		packet.mExpectPayload = host_ptr != nullptr;

		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream, CL_DEVICE_NOT_AVAILABLE);

		stream.write(packet);
		if (host_ptr) stream.write<PayloadPtr<>>({host_ptr, size});
		stream.flush();
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return gConnection.getOrInsertObject<MemObject>(stream.read<IDPacket>());
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_mem CL_API_CALL
clCreateSubBuffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type,
                  const void* buffer_create_info, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
	if (buffer == nullptr) ReturnError(CL_INVALID_VALUE);
	if (buffer_create_info == nullptr) ReturnError(CL_INVALID_VALUE);

	try {
		CreateSubBuffer packet;
		packet.mBufferID = GetID(buffer);
		packet.mFlags = flags;
		packet.mCreateType = buffer_create_type;
		const cl_buffer_region& region = *(reinterpret_cast<const cl_buffer_region*>(buffer_create_info));
		packet.mOffset = region.origin;
		packet.mSize = region.size;

		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream, CL_DEVICE_NOT_AVAILABLE);
		stream.write(packet).flush();
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return gConnection.getOrInsertObject<MemObject>(stream.read<IDPacket>());
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clRetainMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_VALUE;

	try {
		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_SUCCESS);
		stream.write<Retain>({'M', GetID(memobj)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e) {
		return e.mData;
	} catch (...) {
	}
	return CL_SUCCESS;
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clReleaseMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_VALUE;

	try {
		auto contextLock(gConnection.getLock());
		GetStream(stream, CL_SUCCESS);
		stream.write<Release>({'M', GetID(memobj)});
		stream.flush();
		stream.read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
	}

	return CL_SUCCESS;
}
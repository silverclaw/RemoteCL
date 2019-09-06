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
#include "apiutil.h"
#include "memmapping.h"
#include "packets/refcount.h"
#include "packets/memory.h"
#include "packets/IDs.h"
#include "packets/payload.h"
#include "packets/commands.h"

using namespace RemoteCL;
using namespace RemoteCL::Client;


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

		auto conn = gConnection.get();

		conn->write(E);
		if (num_events_in_wait_list) {
			conn->write(eventList);
		}
		conn->flush();

		if (event) *event = conn.registerID<Event>(conn->read<IDPacket>());
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

SO_EXPORT CL_API_ENTRY void* CL_API_CALL
clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
                   cl_map_flags map_flags, size_t offset, size_t size,
                   cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                   cl_event* event, cl_int *errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (buffer == nullptr) ReturnError(CL_INVALID_MEM_OBJECT)

	void* ptr = nullptr;

	try {
		auto conn = gConnection.get();
		auto& buf = conn.registerBufferMapping(GetID(buffer));

		buf.data.reset(new uint8_t[size]);
		buf.flags = map_flags;
		buf.offset = offset;
		buf.size = size;
		ptr = reinterpret_cast<void*>(buf.data.get());
	} catch (...) {
		ReturnError(CL_OUT_OF_HOST_MEMORY)
	}

	cl_int ret = CL_SUCCESS;

	if (map_flags & CL_MAP_READ) {
		ret = clEnqueueReadBuffer(command_queue, buffer, blocking_map, offset, size, ptr,
                                  num_events_in_wait_list, event_wait_list, event);
		if (ret != CL_SUCCESS) {
			ReturnError(ret)
		}
	}

	ret = clRetainMemObject(buffer);
	if (errcode_ret) *errcode_ret = ret;

	return ptr;
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

		auto conn = gConnection.get();

		conn->write(E);
		if (num_events_in_wait_list) {
			conn->write(eventList);
		}
		conn->flush();

		if (event) *event = conn.registerID<Event>(conn->read<IDPacket>());
		conn->read<PayloadInto<>>({ptr});
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
clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                        const size_t* buffer_origin, const size_t* host_origin,
                        const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
                        size_t host_row_pitch, size_t host_slice_pitch, void* ptr,
                        cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                        cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (buffer == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		ReadBufferRect E;

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
		E.mBufferOrigin[0] = buffer_origin[0];
		E.mBufferOrigin[1] = buffer_origin[1];
		E.mBufferOrigin[2] = buffer_origin[2];
		E.mHostOrigin[0] = host_origin[0];
		E.mHostOrigin[1] = host_origin[1];
		E.mHostOrigin[2] = host_origin[2];
		E.mRegion[0] = region[0];
		E.mRegion[1] = region[1];
		E.mRegion[2] = region[2];
		E.mBufferRowPitch = buffer_row_pitch;
		E.mBufferRowPitch = buffer_slice_pitch;
		E.mBufferRowPitch = host_row_pitch;
		E.mBufferRowPitch = host_slice_pitch;
		E.mQueueID = GetID(command_queue);

		auto conn = gConnection.get();

		conn->write(E);
		if (num_events_in_wait_list) {
			conn->write(eventList);
		}
		conn->flush();

		if (event) *event = conn.registerID<Event>(conn->read<IDPacket>());
		conn->read<PayloadInto<>>({ptr});
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
clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr,
                        cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                        cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_MEM_OBJECT;

	const CLMappedBuffer *buf = nullptr;
	cl_int ret;

	{
		auto conn = gConnection.get();
		buf = conn.getBufferMapping(mapped_ptr);
	}

	if (buf == nullptr) return CL_INVALID_VALUE;

	if (buf->flags & CL_MAP_WRITE) {
		ret = clEnqueueWriteBuffer(command_queue, memobj, true, buf->offset,
                                   buf->size, buf->data.get(),
                                   num_events_in_wait_list, event_wait_list, event);
		if (ret != CL_SUCCESS) {
			return ret;
		}
	}

	gConnection.get().unregisterBufferMapping(mapped_ptr);

	return clReleaseMemObject(memobj);
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

		auto conn = gConnection.get();

		conn->write(E);
		if (num_events_in_wait_list) {
			conn->write(eventList);
		}
		conn->write<PayloadPtr<>>({ptr, size});
		conn->flush();

		if (event) *event = conn.registerID<Event>(conn->read<IDPacket>());
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

		auto conn = gConnection.get();

		conn->write(packet);
		if (host_ptr) conn->write<PayloadPtr<>>({host_ptr, size});
		conn->flush();
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return conn.getOrInsertObject<MemObject>(conn->read<IDPacket>());
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

		auto conn = gConnection.get();

		conn->write(packet).flush();
		if (errcode_ret) *errcode_ret = CL_SUCCESS;
		return conn.getOrInsertObject<MemObject>(conn->read<IDPacket>());
	} catch (const ErrorPacket& e) {
		ReturnError(e.mData);
	} catch (...) {
		ReturnError(CL_DEVICE_NOT_AVAILABLE);
	}
}

SO_EXPORT CL_API_ENTRY cl_int CL_API_CALL
clGetMemObjectInfo(cl_mem memobj, cl_mem_info param_name, size_t param_value_size,
                   void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		IDParamPair<PacketType::GetMemObjInfo> query;
		query.mID = GetID(memobj);
		query.mData = param_name;

		auto conn = gConnection.get();

		conn->write(query).flush();

		switch (param_name) {
			// These queries must be ID-translated.
			case CL_MEM_CONTEXT: {
				Context& id = conn.getOrInsertObject<Context>(conn->read<IDPacket>());
				Store<cl_context>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}
			case CL_MEM_ASSOCIATED_MEMOBJECT: {
				MemObject& id = conn.getOrInsertObject<MemObject>(conn->read<IDPacket>());
				Store<cl_mem>(id, param_value, param_value_size, param_value_size_ret);
				break;
			}

			// All others, just memcpy whatever the server returned.
			default: {
				// The only other queries are sizes and refcount properties,
				// which should fit in 4 bytes.
				Payload<uint8_t> payload = conn->read<Payload<uint8_t>>();
				if (param_value_size_ret) *param_value_size_ret = payload.mData.size();
				if (param_value && param_value_size >= payload.mData.size()) {
					std::memcpy(param_value, payload.mData.data(), payload.mData.size());
				}
			}
			break;
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
clRetainMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_VALUE;

	try {
		auto conn = gConnection.get();
		conn->write<Retain>({'M', GetID(memobj)});
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
clReleaseMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	if (memobj == nullptr) return CL_INVALID_VALUE;

	try {
		auto conn = gConnection.get();
		conn->write<Release>({'M', GetID(memobj)});
		conn->flush();
		conn->read<SuccessPacket>();
	} catch (const ErrorPacket& e){
		return e.mData;
	} catch (...) {
		return CL_DEVICE_NOT_AVAILABLE;
	}

	return CL_SUCCESS;
}

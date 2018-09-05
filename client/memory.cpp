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
clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
                   const size_t* origin, const size_t* region,
                   size_t row_pitch, size_t slice_pitch,  void* ptr,
                   cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
                   cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	if (command_queue == nullptr) return CL_INVALID_COMMAND_QUEUE;
	if (image == nullptr) return CL_INVALID_MEM_OBJECT;

	try {
		ReadImage E;

		E.mRowPitch = row_pitch;
		E.mSlicePitch = slice_pitch;
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

		E.mOrigin[0] = origin[0];
		E.mOrigin[1] = origin[1];
		E.mOrigin[2] = origin[2];
		E.mRegion[0] = region[0];
		E.mRegion[1] = region[1];
		E.mRegion[2] = region[2];

		E.mImageID = GetID(image);
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

SO_EXPORT CL_API_ENTRY cl_mem CL_API_CALL
clCreateImage(cl_context context, cl_mem_flags flags,
              const cl_image_format* image_format, const cl_image_desc* image_desc,
              void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
	if (context == nullptr) ReturnError(CL_INVALID_CONTEXT);
	if (image_desc == nullptr) ReturnError(CL_INVALID_IMAGE_DESCRIPTOR);
	if (image_format == nullptr) ReturnError(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
	// We don't support host ptr yet.
	if (host_ptr != nullptr) ReturnError(CL_INVALID_OPERATION);

	try {
		CreateImage packet;
		packet.mFlags = flags;
		packet.mChannelOrder = image_format->image_channel_order;
		packet.mChannelType = image_format->image_channel_data_type;
		packet.mImageType = image_desc->image_type;
		packet.mWidth = image_desc->image_width;
		packet.mHeight = image_desc->image_height;
		packet.mDepth = image_desc->image_depth;
		packet.mArraySize = image_desc->image_array_size;
		packet.mRowPitch = image_desc->image_row_pitch;
		packet.mSlicePitch = image_desc->image_slice_pitch;
		packet.mMipLevels = image_desc->num_mip_levels;
		packet.mSamples = image_desc->num_samples;
		packet.mContextID = GetID(context);

		auto contextLock(gConnection.getLock());
		GetStreamErrRet(stream, CL_DEVICE_NOT_AVAILABLE);

		stream.write(packet);
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
clCreateImage2D(cl_context context, cl_mem_flags flags,
                const cl_image_format* image_format,
                size_t image_width, size_t image_height, size_t image_row_pitch,
                void* host_ptr, cl_int* errcode_ret)
{
	cl_image_desc desc = {};
	desc.image_width = image_width;
	desc.image_height = image_height;
	desc.image_row_pitch = image_row_pitch;
	desc.image_type = CL_MEM_OBJECT_IMAGE2D;
	return clCreateImage(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

SO_EXPORT CL_API_ENTRY cl_mem CL_API_CALL
clCreateImage3D(cl_context context, cl_mem_flags flags,
                const cl_image_format* image_format,
                size_t image_width, size_t image_height, size_t image_depth,
                size_t image_row_pitch, size_t image_slice_pitch,
                void* host_ptr, cl_int* errcode_ret)
{
	cl_image_desc desc = {};
	desc.image_width = image_width;
	desc.image_height = image_height;
	desc.image_depth = image_depth;
	desc.image_row_pitch = image_row_pitch;
	desc.image_slice_pitch = image_slice_pitch;
	desc.image_type = CL_MEM_OBJECT_IMAGE3D;
	return clCreateImage(context, flags, image_format, &desc, host_ptr, errcode_ret);
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

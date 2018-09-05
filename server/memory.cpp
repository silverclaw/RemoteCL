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

#include "instance.h"

#include "CL/cl.h"

#include "packets/memory.h"
#include "packets/IDs.h"
#include "packets/commands.h"
#include "packets/payload.h"

#include "hints.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

void ServerInstance::createImage()
{
	CreateImage packet = mStream.read<CreateImage>();

	cl_context context = getObj<cl_context>(packet.mContextID);
	cl_mem_flags flags = packet.mFlags;
	cl_image_format	format;
	format.image_channel_order = packet.mChannelOrder;
	format.image_channel_data_type = packet.mChannelType;
	cl_image_desc desc = {};
	desc.image_type = packet.mImageType;
	desc.image_width = packet.mWidth;
	desc.image_height = packet.mHeight;
	desc.image_depth = packet.mDepth;
	desc.image_array_size = packet.mArraySize;
	desc.image_row_pitch = packet.mRowPitch;
	desc.image_slice_pitch = packet.mSlicePitch;
	desc.num_mip_levels = packet.mMipLevels;
	desc.num_samples = packet.mSamples;

	cl_int errCode = CL_SUCCESS;
	cl_mem image = clCreateImage(context, flags, &format, &desc, nullptr, &errCode);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
	} else {
		mStream.write<IDPacket>(getIDFor(image));
	}
}

void ServerInstance::createBuffer()
{
	CreateBuffer packet = mStream.read<CreateBuffer>();

	cl_context context = getObj<cl_context>(packet.mContextID);
	cl_mem_flags flags = packet.mFlags;
	std::size_t size = packet.mSize;
	std::vector<uint8_t> hostData;
	void* hostDataPtr = nullptr;
	if (packet.mExpectPayload) {
		Payload<> payload = mStream.read<Payload<>>();
		hostData = std::move(payload.mData);
		hostDataPtr = hostData.data();
	}
	cl_int errCode = CL_SUCCESS;
	cl_mem buffer = clCreateBuffer(context, flags, size, hostDataPtr, &errCode);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
	} else {
		mStream.write<IDPacket>(getIDFor(buffer));
	}
}

void ServerInstance::createSubBuffer()
{
	CreateSubBuffer packet = mStream.read<CreateSubBuffer>();

	cl_buffer_region region;
	region.size = packet.mSize;
	region.origin = packet.mOffset;
	cl_mem buffer = getObj<cl_mem>(packet.mBufferID);
	cl_mem_flags flags = packet.mFlags;
	cl_buffer_create_type type = packet.mCreateType;
	cl_int errCode = CL_SUCCESS;
	cl_mem sub = clCreateSubBuffer(buffer, flags, type, &region, &errCode);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
	} else {
		mStream.write<IDPacket>(getIDFor(sub));
	}
}

void ServerInstance::readImage()
{
	ReadImage packet = mStream.read<ReadImage>();

	// We must figure out how much memory to allocate for the data payload.
	// This is dependent on the pixel size of the image, which we get here:
	std::size_t pixelSize = 0;
	std::size_t retSize = 0;
	cl_mem image = getObj<cl_mem>(packet.mImageID);
	cl_int err = clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, sizeof(std::size_t), &pixelSize, &retSize);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}

	std::vector<cl_event> events;
	if (packet.mExpectEventList) {
		IDListPacket eventList = mStream.read<IDListPacket>();
		events.reserve(eventList.mIDs.size());
		for (IDType id : eventList.mIDs) {
			events.push_back(getObj<cl_event>(id));
		}
	}

	cl_command_queue queue = getObj<cl_command_queue>(packet.mQueueID);
	std::size_t origin[3] = {packet.mOrigin[0], packet.mOrigin[1], packet.mOrigin[2]};
	std::size_t region[3] = {packet.mRegion[0], packet.mRegion[1], packet.mRegion[2]};

	cl_event retEvent;
	cl_event* event = packet.mWantEvent ? &retEvent : nullptr;

	Payload<> data;
	data.mData.resize(pixelSize * region[0] * region[1] * region[2]);
	void* ptr = data.mData.data();

	err = clEnqueueReadImage(queue, image, packet.mBlock, origin, region,
	                         packet.mRowPitch, packet.mSlicePitch, ptr,
	                         events.size(), events.data(), event);

	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	if (packet.mWantEvent) {
		mStream.write<IDPacket>(getIDFor(retEvent));
	}

	mStream.write(data);
}

void ServerInstance::readBuffer()
{
	ReadBuffer packet = mStream.read<ReadBuffer>();

	std::vector<cl_event> events;
	if (packet.mExpectEventList) {
		IDListPacket eventList = mStream.read<IDListPacket>();
		events.reserve(eventList.mIDs.size());
		for (IDType id : eventList.mIDs) {
			events.push_back(getObj<cl_event>(id));
		}
	}

	std::vector<uint8_t> data;
	data.resize(packet.mSize);

	cl_event retEvent;
	cl_event* event = packet.mWantEvent ? &retEvent : nullptr;
	cl_mem buffer = getObj<cl_mem>(packet.mBufferID);
	cl_command_queue queue = getObj<cl_command_queue>(packet.mQueueID);
	// We don't support background reading of buffer data, so block the command.
	cl_int err = clEnqueueReadBuffer(queue, buffer, true, packet.mOffset,
	                                 packet.mSize, data.data(),
	                                 events.size(), events.data(), event);

	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	if (packet.mWantEvent) {
		mStream.write<IDPacket>(getIDFor(retEvent));
	}
	mStream.write<PayloadPtr<>>({data.data(), data.size()});
}

void ServerInstance::writeBuffer()
{
	WriteBuffer packet = mStream.read<WriteBuffer>();

	std::vector<cl_event> events;
	if (packet.mExpectEventList) {
		IDListPacket eventList = mStream.read<IDListPacket>();
		events.reserve(eventList.mIDs.size());
		for (IDType id : eventList.mIDs) {
			events.push_back(getObj<cl_event>(id));
		}
	}

	Payload<> data = mStream.read<Payload<>>();

	cl_event retEvent;
	cl_event* event = packet.mWantEvent ? &retEvent : nullptr;
	cl_mem buffer = getObj<cl_mem>(packet.mBufferID);
	cl_command_queue queue = getObj<cl_command_queue>(packet.mQueueID);
	cl_int err = clEnqueueWriteBuffer(queue, buffer, packet.mBlock, packet.mOffset,
	                                  data.mData.size(), data.mData.data(),
	                                  events.size(), events.data(), event);

	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	if (packet.mWantEvent) {
		mStream.write<IDPacket>(getIDFor(retEvent));
	}
	mStream.write<SuccessPacket>({});
}

void ServerInstance::fillBuffer()
{
	FillBuffer packet = mStream.read<FillBuffer>();

	std::vector<cl_event> events;
	if (packet.mExpectEventList) {
		IDListPacket eventList = mStream.read<IDListPacket>();
		events.reserve(eventList.mIDs.size());
		for (IDType id : eventList.mIDs) {
			events.push_back(getObj<cl_event>(id));
		}
	}

	cl_event retEvent;
	cl_event* event = packet.mWantEvent ? &retEvent : nullptr;
	cl_mem buffer = getObj<cl_mem>(packet.mBufferID);
	cl_command_queue queue = getObj<cl_command_queue>(packet.mQueueID);
	cl_int err = clEnqueueFillBuffer(queue, buffer, packet.mPattern.data(),
	                                 packet.mPatternSize, packet.mOffset, packet.mSize,
	                                 events.size(), events.data(), event);

	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	if (packet.mWantEvent) {
		mStream.write<IDPacket>(getIDFor(retEvent));
	}
	mStream.write<SuccessPacket>({});
}

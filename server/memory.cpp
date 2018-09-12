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

void ServerInstance::getMemObjInfo()
{
	auto query = mStream.read<IDParamPair<PacketType::GetMemObjInfo>>();

	cl_mem obj = getObj<cl_mem>(query.mID);
	cl_mem_info param = query.mData;
	std::size_t retSize = 0;

	switch (query.mData) {
		case CL_MEM_CONTEXT: {
			cl_context context;
			cl_int err = clGetMemObjectInfo(obj, param, sizeof(cl_context), &context, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>({getIDFor(context)});
			break;
		}
		case CL_MEM_ASSOCIATED_MEMOBJECT: {
			cl_mem parent;
			cl_int err = clGetMemObjectInfo(obj, param, sizeof(cl_mem), &parent, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>({getIDFor(parent)});
			break;
		}

		// All others, just memcpy whatever the server returned.
		default: {
			cl_int err = clGetMemObjectInfo(obj, param, 0, nullptr, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			std::vector<uint8_t> data;
			data.resize(retSize);

			err = clGetMemObjectInfo(obj, param, data.size(), data.data(), &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write(PayloadPtr<uint8_t>(data.data(), retSize));
			break;
		}
	}
}

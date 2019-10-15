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

#include "packets/callbacks.h"
#include "packets/commands.h"
#include "packets/IDs.h"
#include "packets/event.h"

#include "hints.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

namespace
{
struct CallbackBlock
{
	ServerInstance *instance;
	/// Client-side ID for the callback.
	IDType ID;
};

void CL_CALLBACK EventCallback(cl_event, cl_int code, void* data)
{
	CallbackBlock *block = reinterpret_cast<CallbackBlock*>(data);
	block->instance->triggerEventCallback(code, block->ID);
	// Block should probably be deleted here - we're currently leaking it.
	// Is it possible for the same event to be executed more than once?
}
}

void ServerInstance::triggerEventCallback(cl_int code, IDType callbackID) noexcept
{
	std::unique_lock<std::mutex> lock(mEventMutex);
	if (!mEventStream) return;
	// Tell the client there's an incoming event.
	mEventStream->write<CallbackTriggerPacket>(callbackID);
	// The client will dipatch to a handler which will receive this packet:
	mEventStream->write<TriggerEventCallback>(code).flush();
}

void ServerInstance::registerEventCallback()
{
	try {
		RegisterEventCallback P = mStream.read<RegisterEventCallback>();

		cl_event event = getObj<cl_event>(P.mEventID);
		// Create an event block
		CallbackBlock *block = new CallbackBlock;
		block->ID = P.mCallbackID;
		block->instance = this;

		cl_int err = clSetEventCallback(event, P.mCBType, EventCallback, reinterpret_cast<void*>(block));
		if (Unlikely(err != CL_SUCCESS)) {
			mStream.write<ErrorPacket>(err);
		}
		mStream.write<SuccessPacket>({});
	} catch (...) {
		mStream.write<ErrorPacket>(CL_OUT_OF_HOST_MEMORY);
	}
}

void ServerInstance::enqueueKernel()
{
	EnqueueKernel E = mStream.read<EnqueueKernel>();
	std::vector<cl_event> events;

	if (E.mExpectEventList) {
		IDListPacket eventList = mStream.read<IDListPacket>();
		events.reserve(eventList.mIDs.size());
		for (IDType id : eventList.mIDs) {
			events.push_back(getObj<cl_event>(id));
		}
	}

	cl_kernel kernel = getObj<cl_kernel>(E.mKernelID);
	cl_command_queue queue = getObj<cl_command_queue>(E.mQueueID);
	cl_event command;
	cl_event* retEvent = E.mWantEvent ? &command : nullptr;

	std::size_t globalSize[3] = {E.mGlobalSize[0], E.mGlobalSize[1], E.mGlobalSize[2]};
	std::size_t globalOffset[3] = {E.mGlobalOffset[0], E.mGlobalOffset[1], E.mGlobalOffset[2]};
	std::size_t localSizeBuffer[3] = {E.mLocalSize[0], E.mLocalSize[1], E.mLocalSize[2]};
	// If the client sends over a "0" as the workgroup size, assume none was specified by the client,
	// ie, a nullptr was given to the argument, which we replicate here.
	std::size_t* localSize = E.mLocalSize[0] == 0 ? nullptr : localSizeBuffer;

	cl_int retCode = clEnqueueNDRangeKernel(queue, kernel, E.mWorkDim,
	                                        globalOffset, globalSize, localSize,
	                                        events.size(), events.data(), retEvent);
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
		return;
	}

	if (retEvent) {
		mStream.write<IDPacket>(getIDFor(*retEvent));
	}
	mStream.write<SuccessPacket>({});
}

void ServerInstance::createUserEvent()
{
	CreateUserEvent P = mStream.read<CreateUserEvent>();
	cl_context context = getObj<cl_context>(P);

	cl_int err = CL_SUCCESS;
	cl_event event = clCreateUserEvent(context, &err);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	mStream.write<IDPacket>(getIDFor(event));
}

void ServerInstance::setUserEventStatus()
{
	SetUserEventStatus packet = mStream.read<SetUserEventStatus>();
	cl_event event = getObj<cl_event>(packet.mID);
	cl_int err = clSetUserEventStatus(event, packet.mData);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

void ServerInstance::getEventInfo()
{
	GetEventInfo packet = mStream.read<GetEventInfo>();
	cl_event event = getObj<cl_event>(packet.mID);

	union
	{
		cl_context context;
		cl_command_queue queue;
		cl_command_type type;
		cl_int eventStatus;
		cl_uint referenceCount;
	} data;
	size_t replySize = 0;
	// All replies to getEventInfo will be at most sizeof(cl_ulong).
	cl_int errCode = clGetEventInfo(event, packet.mData, sizeof(data), &data, &replySize);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}

	// Data is encoded differently depending on request.
	switch (packet.mData) {
		case CL_EVENT_CONTEXT:
			mStream.write<IDPacket>(getIDFor(data.context));
			break;

		case CL_EVENT_COMMAND_QUEUE:
			mStream.write<IDPacket>(getIDFor(data.queue));
			break;
		case CL_EVENT_COMMAND_TYPE:
			mStream.write<SimplePacket<PacketType::Payload, int32_t>>(data.type);
			break;
		case CL_EVENT_COMMAND_EXECUTION_STATUS:
			mStream.write<SimplePacket<PacketType::Payload, int32_t>>(data.eventStatus);
			break;
		case CL_EVENT_REFERENCE_COUNT:
			mStream.write<SimplePacket<PacketType::Payload, uint32_t>>(data.referenceCount);
			break;
		default:
			// Should not happen - invalid request.
			mStream.write<ErrorPacket>(CL_INVALID_OPERATION);
			break;
	}
}

void ServerInstance::getEventProfilingInfo()
{
	GetEventProfilingInfo packet = mStream.read<GetEventProfilingInfo>();
	cl_event event = getObj<cl_event>(packet.mID);

	SimplePacket<PacketType::Payload, uint64_t> reply;
	size_t replySize = 0;
	// All replies to getEventProfilingInfo will be sizeof(cl_ulong)
	cl_int errCode = clGetEventProfilingInfo(event, packet.mData, sizeof(cl_ulong), &reply.mData, &replySize);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}

	mStream.write(reply);
}

void ServerInstance::waitForEvents()
{
	mStream.read<WaitForEvents>();
	std::vector<cl_event> events;
	IDListPacket eventList = mStream.read<IDListPacket>();
	events.reserve(eventList.mIDs.size());
	for (IDType id : eventList.mIDs) {
		events.push_back(getObj<cl_event>(id));
	}
	cl_int retCode = clWaitForEvents(events.size(), events.data());
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

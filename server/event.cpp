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

#include "packets/commands.h"
#include "packets/IDs.h"
#include "packets/event.h"

#include "hints.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

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

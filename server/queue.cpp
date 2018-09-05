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

#include <vector>

#include "CL/cl.h"

#include "hints.h"
#include "packets/queue.h"
#include "packets/IDs.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;


void ServerInstance::createQueue()
{
	CreateQueue packet = mStream.read<CreateQueue>();

	std::vector<cl_queue_properties> properties;

	cl_context context = getObj<cl_context>(packet.mContext);
	cl_device_id device = getObj<cl_device_id>(packet.mDevice);
	cl_int retCode = CL_SUCCESS;
	cl_command_queue queue = clCreateCommandQueue(context, device, packet.mProp, &retCode);
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
	} else {
		mStream.write<IDPacket>({getIDFor(queue)});
	}
}

void ServerInstance::createQueueWithProp()
{
#if (CL_TARGET_OPENCL_VERSION >= 200)
	CreateQueueWithProp packet = mStream.read<CreateQueueWithProp>();

	std::vector<cl_queue_properties> properties;
	if (!packet.mProperties.empty()) {
		for (auto& p : packet.mProperties) {
			properties.push_back(p);
		}
		// Insert the expected 0 terminator.
		properties.push_back(0);
	}

	cl_queue_properties* prop = properties.empty() ? nullptr : properties.data();

	cl_context context = getObj<cl_context>(packet.mContext);
	cl_device_id device = getObj<cl_device_id>(packet.mDevice);
	cl_int retCode = CL_SUCCESS;
	cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, prop, &retCode);
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
	} else {
		mStream.write<IDPacket>({getIDFor(queue)});
	}
#else
	// Support for this was disabled.
	mStream.write<ErrorPacket>(CL_INVALID_OPERATION);
#endif
}

void ServerInstance::flushQueue()
{
	// Queue finish may take a long time.
	// Hopefully, the system won't close the socket.
	QFlushPacket finish = mStream.read<QFlushPacket>();
	cl_command_queue queue = getObj<cl_command_queue>(finish.mData);
	cl_int err = clFlush(queue);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

void ServerInstance::finishQueue()
{
	// Queue finish may take a long time.
	// Hopefully, the system won't close the socket.
	QFinishPacket finish = mStream.read<QFinishPacket>();
	cl_command_queue queue = getObj<cl_command_queue>(finish.mData);
	cl_int err = clFinish(queue);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

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

#include <iostream>

#include "CL/cl.h"

#include "hints.h"
#include "packets/callbacks.h"
#include "packets/packet.h"
#include "packets/version.h"
#include "packets/platform.h"
#include "packets/device.h"
#include "packets/IDs.h"
#include "packets/payload.h"
#include "packets/event.h"
#include "packets/terminate.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

ServerInstance::ServerInstance(Socket socket) : mStream(std::move(socket))
{
	mStream.write<VersionPacket>({});
	mStream.flush();
}

void ServerInstance::sendPlatformList()
{
	mStream.read<GetPlatformIDs>();
	cl_uint platformCount = 0;
	cl_int err = clGetPlatformIDs(0, nullptr, &platformCount);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	std::vector<cl_platform_id> platforms;
	platforms.resize(platformCount);
	err = clGetPlatformIDs(platforms.size(), platforms.data(), &platformCount);

	IDListPacket list;
	list.mIDs.reserve(platformCount);
	for (cl_platform_id id : platforms) {
		list.mIDs.push_back(getIDFor(id));
	}

	mStream.write(list);
}

void ServerInstance::getPlatformInfo()
{
	GetPlatformInfo packet = mStream.read<GetPlatformInfo>();
	cl_platform_id platform = getObj<cl_platform_id>(packet.mID);

	size_t replySize = 0;
	cl_int errCode = clGetPlatformInfo(platform, packet.mData, 0, nullptr, &replySize);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}

	Payload<uint8_t> reply;
	reply.mData.resize(replySize);
	errCode = clGetPlatformInfo(platform, packet.mData, replySize, reply.mData.data(), &replySize);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}
	mStream.write(reply);
}

void ServerInstance::createEventStream()
{
	// Discard the packet - no payload.
	mStream.read<OpenEventStream>();

	std::unique_lock<std::mutex> lock(mEventMutex);

	// Attempt to open a listening socket on a random port.
	// Try 16 times.
	uint8_t triesLeft = 16;
	do {
		triesLeft--;
		try {
			// Use the range 49152–65535 (ephemeral ports) which IANA does not reserve.
			// This reduces the probability of clashes with other services.
			const uint16_t PortMin = 49152;
			const uint16_t PortMax = 65535;
			uint16_t port = (std::rand()%((PortMax - PortMin) + 1)) + PortMin;
			Socket socket(port);
			// If the socket bind succeeds, tell the client this port.
			// We won't be trying again anymore.
			triesLeft = 0;
			mStream.write<SimplePacket<PacketType::Payload, uint16_t>>(port).flush();
			// There is a chance, however small, that the client will try to connect
			// before we go into "accept()", but there's nothing we can do about it.
			std::unique_ptr<PacketStream> stream(new PacketStream(socket.accept()));
			// If opening the stream succeeded, replace the existing event stream.
			mEventStream = std::move(stream);
			return;
		} catch (const Socket::Error&) {
			// The socket failed to bind, we'll try again (maybe).
		} catch (...) {
			// any other error, we give up.
			break;
		}
	} while (triesLeft != 0);

	// Couldn't open a socket - tell the client to continue without one.
	mStream.write<SimplePacket<PacketType::Payload, uint16_t>>(0);
}

bool ServerInstance::handleNextPacket()
{
	switch (mStream.nextPacketTy()) {
		case PacketType::Terminate:
			std::clog << "Client terminated connection. ";
			if (mEventStream) {
				mEventStream->write<TerminatePacket>({});
				mEventStream.reset();
			}
			return false;

		case PacketType::GetDeviceIDs:
			sendDeviceList();
			break;
		case PacketType::GetDeviceInfo:
			getDeviceInfo();
			break;

		case PacketType::GetPlatformInfo:
			getPlatformInfo();
			break;
		case PacketType::GetPlatformIDs:
			sendPlatformList();
			break;

		case PacketType::CreateContext:
			createContext();
			break;
		case PacketType::CreateContextFromType:
			createContextFromType();
			break;
		case PacketType::GetContextInfo:
			getContextInfo();
			break;
		case PacketType::GetImageFormats:
			getImageFormats();
			break;

		case PacketType::CreateSourceProgram:
			createProgramFromSource();
			break;
		case PacketType::CreateBinaryProgram:
			createProgramFromBinary();
			break;
		case PacketType::BuildProgram:
			buildProgram();
			break;
		case PacketType::BuildInfo:
			getProgramBuildInfo();
			break;
		case PacketType::ProgramInfo:
			getProgramInfo();
			break;

		case PacketType::CreateKernel:
			createKernel();
			break;
		case PacketType::CreateKernels:
			createKernels();
			break;
		case PacketType::CloneKernel:
			cloneKernel();
			break;
		case PacketType::SetKernelArg:
			setKernelArg();
			break;
		case PacketType::KernelWGInfo:
			getKernelWGInfo();
			break;
		case PacketType::KernelArgInfo:
			getKernelArgInfo();
			break;
		case PacketType::KernelInfo:
			getKernelInfo();
			break;

		case PacketType::CreateQueue:
			createQueue();
			break;
		case PacketType::CreateQueueWithProp:
			createQueueWithProp();
			break;
		case PacketType::GetQueueInfo:
			getQueueInfo();
			break;

		case PacketType::Flush:
			flushQueue();
			break;
		case PacketType::Finish:
			finishQueue();
			break;

		case PacketType::CreateBuffer:
			createBuffer();
			break;
		case PacketType::CreateSubBuffer:
			createSubBuffer();
			break;
		case PacketType::ReadBuffer:
			readBuffer();
			break;
		case PacketType::ReadBufferRect:
			readBufferRect();
			break;
		case PacketType::WriteBuffer:
			writeBuffer();
			break;
		case PacketType::FillBuffer:
			fillBuffer();
			break;
		case PacketType::GetMemObjInfo:
			getMemObjInfo();
			break;

		case PacketType::CreateImage:
			createImage();
			break;
		case PacketType::ReadImage:
			readImage();
			break;
		case PacketType::WriteImage:
			writeImage();
			break;
		case PacketType::GetImageInfo:
			getImageInfo();
			break;

		case PacketType::EnqueueKernel:
			enqueueKernel();
			break;

		case PacketType::WaitEvents:
			waitForEvents();
			break;
		case PacketType::CreateUserEvent:
			createUserEvent();
			break;
		case PacketType::GetEventProfilingInfo:
			getEventProfilingInfo();
			break;
		case PacketType::GetEventInfo:
			getEventInfo();
			break;
		case PacketType::SetUserEventStatus:
			setUserEventStatus();
			break;

		case PacketType::Release:
			handleRelease();
			break;
		case PacketType::Retain:
			handleRetain();
			break;

		case PacketType::RegisterEventCallback:
			registerEventCallback();
			break;

		case PacketType::EventStreamOpen:
			// This should only happen once, but not a real issue if called multiple times.
			createEventStream();
			break;

		case PacketType:: Payload:
			// Payloads need to be handled by the associated command processor.
			assert(false && "Unexpected payload");

		case PacketType::ID:
		case PacketType::Success:
		case PacketType::Error:
		case PacketType::IDList:
		case PacketType::Version:
		case PacketType::CallbackTrigger:
		case PacketType::EventCallbackTrigger:
			// The client shouldn't send these packet types.
			std::cerr << "Unexpected packet\n";
			// This will terminate the connection with the client.
			return false;
	}

	// By default, continue handling packets.
	return true;
}

void ServerInstance::run()
{
	bool shouldContinue = true;
	do {
		try {
			shouldContinue = handleNextPacket();
		} catch (const std::bad_alloc&) {
			// Technically it's server memory as the client might actually have enough.
			mStream.write<ErrorPacket>(CL_OUT_OF_HOST_MEMORY);
		}
		mStream.flush();
	} while (shouldContinue);
}

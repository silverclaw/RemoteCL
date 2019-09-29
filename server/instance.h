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

#if !defined(REMOTECL_SERVER_INSTANCE_H)
#define REMOTECL_SERVER_INSTANCE_H

#include <functional>
#include <list>
#include <vector>

#include "CL/cl.h"

#include "idtype.h"
#include "socket.h"
#include "packetstream.h"

namespace RemoteCL
{
namespace Server
{
class ServerInstance
{
public:
	ServerInstance(Socket commandSocket, Socket eventSocket);

	void run();

private:
	/// Waits for the next packet. Called continuously as long as it return true;
	bool handleNextPacket();

	void sendPlatformList();
	void getPlatformInfo();
	void sendDeviceList();
	void getDeviceInfo();

	void handleRetain();
	void handleRelease();

	void createContext();
	void createContextFromType();
	void getContextInfo();
	void getImageFormats();

	void createQueue();
	void createQueueWithProp();
	void getQueueInfo();
	void flushQueue();
	void finishQueue();

	void createProgramFromSource();
	void createProgramFromBinary();
	void buildProgram();
	void getProgramBuildInfo();
	void getProgramInfo();

	void createKernel();
	void createKernels();
	void cloneKernel();
	void setKernelArg();
	void getKernelInfo();
	void getKernelArgInfo();
	void getKernelWGInfo();

	void createImage();
	void readImage();
	void writeImage();
	void getImageInfo();

	void createBuffer();
	void createSubBuffer();
	void readBuffer();
	void readBufferRect();
	void writeBuffer();
	void fillBuffer();

	void getMemObjInfo();

	void enqueueKernel();

	void waitForEvents();
	void createUserEvent();
	void getEventInfo();
	void getEventProfilingInfo();
	void setUserEventStatus();
	void setEventCallback();

	PacketStream mStream;
	PacketStream mEventStream;

	struct EventCallback {
		EventCallback(uint32_t id, std::function<void(uint32_t id)> cb) noexcept : ID(id), func(cb) {}

		uint32_t ID;
		std::function<void(uint32_t id)> func;
	};

	std::list<EventCallback> mEventCallbacks;

	/// Retrieves or assigns an ID for this object.
	template<typename T>
	IDType getIDFor(T obj)
	{
		static_assert(std::is_pointer<T>::value, "Must be a pointer type");
		IDType id;
		for (id = 0; id < mObjects.size(); id++) {
			if (mObjects[id] == obj) return id;
		}
		// Not assigned yet.
		mObjects.push_back(obj);
		return id;
	}

	/// Retrieves the object for this ID.
	template<typename T>
	T getObj(IDType id)
	{
		if (id >= mObjects.size()) return nullptr;
		return reinterpret_cast<T>(mObjects[id]);
	}

	/// List of allocated CL objects.
	std::vector<void*> mObjects;
};
} // namespace server
} // namespace RemoteCL

#endif

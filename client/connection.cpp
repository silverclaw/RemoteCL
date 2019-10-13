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

#include "connection.h"

#include "socketstream.h"
#include "objects.h"
#include "packets/callbacks.h"
#include "packets/version.h"
#include "packets/terminate.h"

#include <cstdlib> // getenv
#include <cstring>
#include <thread>

using namespace RemoteCL;
using namespace RemoteCL::Client;

Connection RemoteCL::Client::gConnection;

namespace
{
std::string ParseServerName(const char* name)
{
	const char* end = name;
	if (name[0] == '"') {
		// Search for the next, closing quotation mark.
		do {
			end++;
			if (*end == '\0') return "";
		} while (*end != '"');
	} else {
		// Search for a ";" or end
		do {
			end++;
		} while (*end != '\0' && *end != ';');
	}

	return std::string(name, end-name);
}

/// The connection callback handler.
bool HandleCallbacks(PacketStream& eventStream, std::vector<std::unique_ptr<Callback>>& callbacks,
                     std::mutex& mutex)
{
	switch (eventStream.nextPacketTy()) {
		case PacketType::Terminate:
			return false;

		case PacketType::CallbackTrigger: {
			uint32_t index = eventStream.read<CallbackTriggerPacket>();
			std::unique_lock<std::mutex> lock(mutex);
			if (index >= callbacks.size() || callbacks[index] == nullptr) {
				std::cerr << "Invalid server-side event trigger - ignored." << std::endl;
				return true;
			}
			callbacks[index]->trigger(eventStream);
			// Should we reset callbacks[index] ? callbacks shouldn't be run twice, right?
		}
		break;

		default:
			// We don't handle any other types of packets through this stream.
			std::cerr << "Unexpected packet in event stream" << std::endl;
			return false;
	}
	return true;
}

void CallbackThreadMain(std::unique_ptr<PacketStream>& eventStream, std::vector<std::unique_ptr<Callback>>& callbacks,
                        std::mutex& mutex) noexcept
{
	try {
		while (HandleCallbacks(*eventStream, callbacks, mutex)) {

		}
	} catch (...) {
		// If an exception triggers, we don't really care, the callback handler
		// will terminate. Ensure that the stream itself is reset, so that the
		// host application has the appropriate errors returned.
		std::cerr << "Event Stream terminated." << std::endl;
		// This is very much thread-unsafe, but it is an unlikely case.
		// It'd be overkill to mutex-lock this.
		eventStream.reset();
	}
}
} // anon namespace

// This ought to have been defined through CMake.
// In case something went wrong (or using IDEs)
// define it manually to the default value.
#if !defined(DEFAULT_REMOTE_HOST)
#define DEFAULT_REMOTE_HOST "localhost"
#endif

Connection::Connection() noexcept
{
	try {
		uint16_t port = Socket::DefaultPort;
		std::string serverName = DEFAULT_REMOTE_HOST;

#if defined(WIN32)
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			std::cerr << "Windows sockets failed to initialise." << std::endl;
			return;
		}
#endif

		if (const char* envVar = getenv("REMOTECL")) {
			if (const char* portStr = std::strstr(envVar, "port=")) {
				portStr += 5;
				char* end;
				uint16_t newPort = std::strtoul(portStr, &end, 10);
				if (newPort != 0 && end != portStr) {
					port = newPort;
				}
			}
			if (const char* hostStr = std::strstr(envVar, "host=")) {
				hostStr += 5;
				serverName = ParseServerName(hostStr);
			}
		}

		mStream.reset(new PacketStream(Socket(serverName.c_str(), port)));

		VersionPacket serverVersion = mStream->read<VersionPacket>();
		VersionPacket currentVersion;
		if (!currentVersion.isCompatibleWith(serverVersion)) {
			// The versions are not compatible
			std::cerr << "The RemoteCL server version is not compatible with this client. Disconnecting.\n";
			mStream.reset();
			return;
		}

#if defined(REMOTECL_ENABLE_ASYNC)
		if (serverVersion.eventEnabled()) {
			// Attempt to negotiate a server-side stream.
			mStream->write<OpenEventStream>({}).flush();
			// The server replies with an opened port number, or 0 if an event stream
			// cannot be estabilished.
			const uint16_t eventPort =
				mStream->read<SimplePacket<PacketType::Payload, uint16_t>>();

			if (eventPort != 0) {
				// The server allowed an event port to be opened.
				try {
					mEventStream.reset(new PacketStream(Socket(serverName.c_str(), eventPort)));
					std::thread(CallbackThreadMain,
					            std::ref(mEventStream), std::ref(mCallbacks), std::ref(mCallbackMutex)).detach();
				} catch (...) {
					// Failed to negotiate an event stream.
					std::cerr << "RemoteCL Client event stream could not be opened" << std::endl;
				}
			}
		} else {
			std::clog << "RemoteCL Server does not support event stream." << std::endl;
		}
#endif

		// Preallocate slots for CL objects. This is an estimate of how
		// many objects will be used throughout the lifetime of the connection.
		mObjects.reserve(64);
	}
	catch (...) {
		// TODO: Check errno. Should probably attach it to SocketError.
		std::cerr << "RemoteCL Client failed to initialise." << std::endl;
	}
}

Connection::~Connection()
{
#if defined(WIN32)
	WSACleanup();
#endif

	mObjects.clear();
	if (mStream) {
		try {
			// Not strictly required because the socket will close anyway.
			mStream->write<TerminatePacket>({}).flush();
		} catch (...) {
			// Ignore - the process is terminating.
		}
	}
}

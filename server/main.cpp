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

#include <iostream>
#include <cstring> // std::strcmp
#include <cstdlib> // std::strtoul
#if defined(REMOTECL_SERVER_USE_THREADS)
#include <thread>
#else
#include <unistd.h> // for fork()
#endif
#include <system_error>

#include "instance.h"
#include "socket.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

int main(int argc, char* argv[])
{
	uint16_t port = Socket::DefaultPort;
	for (int i = 1; i < argc; ++i) {
		// The only argument we currently support is "--port".
		if (std::strcmp(argv[i], "--port") == 0) {
			++i;
			if (i == argc) {
				std::cerr << "Missing argument for --port.\n";
				return -1;
			}
			// Parse the next argument for a port number.
			char* end;
			port = std::strtoul(argv[i], &end, 10);
			if (*end != '\0' || end == argv[i]) {
				std::cerr << "Couldn't understand port number " << argv[i] << '\n';
				return -1;
			}
		} else if (std::strcmp(argv[i], "--help") == 0) {
			std::cout << "RemoteCL server binary. Start with:\n";
			std::cout << argv[0] << " [--port number]\n";
			std::cout << "where the default port is " << Socket::DefaultPort << '\n';
			return 0;
		} else {
			std::cerr << "Unknown argument " << argv[i] << "\n";
			return -1;
		}
	}

	std::clog << "Opening server port at " << port << '\n';

	try {
		Socket server(port);

		bool running = true;
		do {
			try {
				Socket client = server.accept();
				std::clog << "Incoming connection from " << client.getPeerName().data << '\n';
#if defined(REMOTECL_SERVER_USE_THREADS)
				std::thread([](Socket socket){ServerInstance(std::move(socket)).run();},
				            std::move(client)).detach();
#else
				pid_t child = fork();
				if (child == 0) {
					// The child must close the server socket.
					server.close();
					// The accept loop no longer makes sense.
					running = false;
					// off we go.
					ServerInstance(std::move(client)).run();
					// This log message does not get printed if the
					// instance dies through an exception.
					std::clog << "Child instance " << getpid() << " exiting.\n";
				} else if (child == -1) {
					// TODO: check errno here.
					std::cerr << "Child fork failed; connection dropped.\n";
				} else {
					std::clog << "Forked connection to PID " << child << '\n';
				}
				// The client socket will be RAII-closed here.
#endif
			}
			catch (const std::system_error& e) {
				std::clog << "System error while starting client handler: " << e.what() << std::endl;
			}
			catch (const Socket::Error&) {
				// Thrown if the client socket fails to start (parent-side)
				// or if the client socket was lost (child-side, from StartInstance).
				std::clog << "Incoming connection lost\n";
			}
		} while (running);
	}
	catch (const Socket::Error&) {
		std::cerr << "Unable to open server socket\n";
		return -1;
	}

	return 0;
}

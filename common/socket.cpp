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

#include "socket.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include "hints.h"

#if defined(WIN32)
	#include <WinSock2.h>
	#include <WS2tcpip.h>
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <netdb.h> // addrinfo
#endif

using namespace RemoteCL;

namespace
{
class AddressInfo
{
public:
	AddressInfo(const char* hostname)
	{
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		int rs = getaddrinfo(hostname, nullptr, &hints, &mInfo);
		if (rs != 0) {
			std::cerr << "Could not resolve " << hostname << std::endl;
			throw Socket::Error();
		}
	}

	addrinfo* getInfo() noexcept { return mInfo; }

	~AddressInfo()
	{
		if (mInfo) freeaddrinfo(mInfo);
	}

private:
	addrinfo* mInfo;
};
}

RemoteCL::Socket::Socket()
{
	int domain = AF_INET;
	int type = SOCK_STREAM;
	int protocol = IPPROTO_TCP;
	mSocket = socket(domain, type, protocol);
	if (mSocket == InvalidSocket) {
		throw Error();
	}
#if !defined(REMOTECL_DISABLE_KEEP_ALIVE)
#if defined(WIN32)
	using ValTy = const char*;
#else // everyone else
	using ValTy = const void*;
#endif // WIN32
	int val = -1;
	int result = setsockopt(mSocket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<ValTy>(&val), sizeof(val));
	if (result != 0) {
		// This isn't a fatal failure; use the socket as-is.
		std::cerr << "Could not enable socket keep-alive." << std::endl;
	}
#endif // REMOTECL_DISABLE_KEEP_ALIVE
}

RemoteCL::Socket::Socket(uint16_t port) : Socket()
{
	sockaddr_in serverAddress = {};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(port);
	if (::bind(mSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) != 0) {
		throw Error();
	}
}

RemoteCL::Socket::Socket(const char* hostname, uint16_t port) : Socket()
{
	// Resolve the hostname.
	AddressInfo host(hostname);

	// Try all the resolved ports in order.
	addrinfo* info = host.getInfo();
	while (info != nullptr) {
		sockaddr_in serverAddress = {};
		serverAddress = *reinterpret_cast<sockaddr_in*>(info->ai_addr);
		serverAddress.sin_port = htons(port);
		if (::connect(mSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == 0) {
			// Connected
			return;
		}
		info = info->ai_next;
	}

	throw Error();
}

RemoteCL::Socket RemoteCL::Socket::accept()
{
	if (listen(mSocket, 5) != 0) {
		throw Error();
	}
	sockaddr_in clientAddress;
	socklen_t clilen = sizeof(clientAddress);
	SocketTy clientSockFD = ::accept(mSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clilen);
	if (clientSockFD < 0) {
		throw Error();
	}
	return Socket(clientSockFD);
}

void RemoteCL::Socket::send(const void* data, std::size_t size)
{
	const auto written = ::send(mSocket, reinterpret_cast<const char*>(data), size, 0);
	if (written < 0) {
		// Socket likely closed.
		throw Error();
	}

	// This assertion might trigger if the socket disconnected.
	// We may want to throw an exception instead.
	assert(static_cast<std::size_t>(written) == size && "Socket buffer full - should retry here");
}

std::size_t RemoteCL::Socket::receive(void* data, std::size_t available)
{
	const auto bytesRead = recv(mSocket, reinterpret_cast<char*>(data), available, 0);
	if (bytesRead < 0) {
		// Socket likely closed.
		throw Error();
	}
	return bytesRead;
}

Socket::PeerName RemoteCL::Socket::getPeerName() noexcept
{
	sockaddr_storage addr;
	sockaddr_in* s4 = reinterpret_cast<sockaddr_in*>(&addr);
	sockaddr_in6* s6 = reinterpret_cast<sockaddr_in6*>(&addr);

	socklen_t len = sizeof(addr);
	getpeername(mSocket, (struct sockaddr*)&addr, &len);

	PeerName name;
	if (addr.ss_family == AF_INET) {
		//port = ntohs(s4->sin_port);
		inet_ntop(AF_INET, &s4->sin_addr, name.data, sizeof(name.data));
	} else if (addr.ss_family == AF_INET6) {
		// port = ntohs(s6->sin6_port);
		inet_ntop(AF_INET6, &s6->sin6_addr, name.data, sizeof(name.data));
	} else {
		assert(false);
		Unreachable();
	}

	return name;
}

void RemoteCL::Socket::close() noexcept
{
	assert(mSocket != InvalidSocket);
#if !defined(WIN32)
#define closesocket(X) ::close(X)
#endif
	closesocket(mSocket);
}

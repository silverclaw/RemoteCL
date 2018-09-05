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

#if defined(REMOTECL_SERVER_USE_THREADS)
#include <thread>
#else
#include <unistd.h>
#endif
#include <iostream>
#include <cstring>

#include "CL/cl.h"

#include "hints.h"
#include "packets/context.h"
#include "packets/IDs.h"
#include "packets/payload.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

namespace
{
void CL_CALLBACK CallbackFn(const char* str, const void*, size_t, void*)
{
	auto id =
#if defined REMOTECL_SERVER_USE_THREADS
		std::this_thread::get_id();
#else
		getpid();
#endif
	std::cerr << "Child " << id << " reported error " << str << '\n';
}
}

void ServerInstance::createContext()
{
	CreateContext packet = mStream.read<CreateContext>();

	std::vector<cl_device_id> devices;
	devices.reserve(packet.mDevices.size());
	for (IDType device : packet.mDevices) {
		devices.push_back(getObj<cl_device_id>(device));
	}

	// Rebuild the context property list.
	std::vector<cl_context_properties> properties;
	properties.reserve(packet.mProperties.size());
	// Properties come in pairs of uint64_ts
	auto it = packet.mProperties.begin();
	auto itEnd = packet.mProperties.end();
	while (it != itEnd) {
		properties.push_back(*it);
		if (*it == CL_CONTEXT_PLATFORM) {
			++it;
			cl_platform_id platformID = getObj<cl_platform_id>(*it);
			properties.push_back(reinterpret_cast<cl_context_properties>(platformID));
		} else {
			++it;
			properties.push_back(*it);
		}
		++it;
	}
	// If properties isn't empty, it must contain a terminating '0'
	if (!properties.empty()) properties.push_back(0);
	const cl_context_properties* p = properties.empty() ? nullptr : properties.data();
	cl_int retCode = CL_SUCCESS;

	cl_context C = clCreateContext(p, devices.size(), devices.data(), CallbackFn,
	                               nullptr, &retCode);
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
	} else {
		mStream.write<IDPacket>(getIDFor(C));
	}
}

void ServerInstance::createContextFromType()
{
	CreateContextFromType packet = mStream.read<CreateContextFromType>();
	// Rebuild the context property list.
	std::vector<cl_context_properties> properties;
	properties.reserve(packet.mProperties.size());
	// Properties come in pairs of uint64_ts
	auto it = packet.mProperties.begin();
	auto itEnd = packet.mProperties.end();
	while (it != itEnd) {
		properties.push_back(*it);
		if (*it == CL_CONTEXT_PLATFORM) {
			++it;
			cl_platform_id platformID = getObj<cl_platform_id>(*it);
			properties.push_back(reinterpret_cast<cl_context_properties>(platformID));
		} else {
			++it;
			properties.push_back(*it);
		}
		++it;
	}
	// If properties isn't empty, it must contain a terminating '0'
	if (!properties.empty()) properties.push_back(0);
	const cl_context_properties* p = properties.empty() ? nullptr : properties.data();
	cl_int retCode = CL_SUCCESS;

	cl_context C = clCreateContextFromType(p, packet.mDeviceType, CallbackFn,
	                                       nullptr, &retCode);
	if (Unlikely(retCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(retCode);
	} else {
		mStream.write<IDPacket>(getIDFor(C));
	}
}

void ServerInstance::getContextInfo()
{
	GetContextInfo info = mStream.read<GetContextInfo>();
	cl_context context = getObj<cl_context>(info.mID);
	cl_context_info name = static_cast<cl_context_info>(info.mData);

	Payload<uint8_t> reply;
	reply.mData.resize(32);
	size_t replySize = 0;
	cl_int errCode = clGetContextInfo(context, name, reply.mData.size(),
	                                  reply.mData.data(), &replySize);

	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}
	if (replySize > reply.mData.size()) {
		// If the size guess was insufficient, retry.
		reply.mData.resize(replySize);
		errCode = clGetContextInfo(context, name, reply.mData.size(),
		                           reply.mData.data(), &replySize);
		assert(errCode == CL_SUCCESS && "Didn't we just try this?");
		assert(replySize == reply.mData.size());
	}

	// Context queries need to be processed if they use IDs.
	if (name == CL_CONTEXT_DEVICES) {
		// Process the returned device list.
		cl_device_id* ids = reinterpret_cast<cl_device_id*>(reply.mData.data());
		std::vector<IDType> mapped(replySize / sizeof(cl_device_id));
		for (IDType& id : mapped) {
			id = getIDFor(*ids);
			++ids;
		}
		// obviously, replace the answer.
		std::memcpy(reply.mData.data(), mapped.data(), mapped.size() * sizeof(IDType));
		reply.mData.resize(mapped.size() * sizeof(IDType));
	} else {
		// Trim the reply
		reply.mData.resize(replySize);
	}

	mStream.write(reply);
}

void ServerInstance::getImageFormats()
{
	GetImageFormats query = mStream.read<GetImageFormats>();
	cl_context context = getObj<cl_context>(query.mContextID);

	cl_uint formatCount = 0;
	cl_int errCode = clGetSupportedImageFormats(context, query.mFlags, query.mImageType, 0, nullptr, &formatCount);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}

	std::vector<cl_image_format> formats;
	formats.resize(formatCount);

	clGetSupportedImageFormats(context, query.mFlags, query.mImageType, formatCount,
	                           formats.data(), &formatCount);
	mStream.write<PayloadPtr<uint16_t>>({formats});
}

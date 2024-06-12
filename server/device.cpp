
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

using namespace RemoteCL;
using namespace RemoteCL::Server;

#include "CL/cl.h"

#include "hints.h"
#include "packets/device.h"
#include "packets/IDs.h"
#include "packets/payload.h"


void ServerInstance::sendDeviceList()
{
	GetDeviceIDs packet = mStream.read<GetDeviceIDs>();
	cl_uint deviceCount = 0;
	std::vector<cl_device_id> devices;
	constexpr uint32_t DeviceCountGuess = 16;
	devices.resize(DeviceCountGuess);
	cl_device_type devTy = packet.mDeviceType;
	cl_platform_id platform = getObj<cl_platform_id>(packet.mPlatformID);

	cl_int err = clGetDeviceIDs(platform, devTy, 0, nullptr, &deviceCount);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}

	devices.resize(deviceCount);
	err = clGetDeviceIDs(platform, devTy, devices.size(), devices.data(), &deviceCount);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	IDListPacket list;
	list.mIDs.reserve(deviceCount);
	for (cl_device_id id : devices) {
		list.mIDs.push_back(getIDFor(id));
	}

	mStream.write(list);
}

void ServerInstance::getDeviceInfo()
{
	GetDeviceInfo query = mStream.read<GetDeviceInfo>();
	cl_device_id device = getObj<cl_device_id>(query.mID);

	switch (query.mData) {
		// These queries must be ID-translated.
		case CL_DEVICE_PLATFORM: {
			cl_platform_id platform = nullptr;
			std::size_t retSize = 0;
			cl_int err = clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(platform), &platform, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>(getIDFor(platform));
			break;
		}
		case CL_DEVICE_PARENT_DEVICE: {
			cl_device_id parent = nullptr;
			std::size_t retSize = 0;
			cl_int err = clGetDeviceInfo(device, CL_DEVICE_PARENT_DEVICE, sizeof(parent), &parent, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>(getIDFor(parent));
			break;
		}

		// These queries must be std::size_t translated.
		case CL_DEVICE_PRINTF_BUFFER_SIZE:
		case CL_DEVICE_IMAGE2D_MAX_WIDTH:
		case CL_DEVICE_IMAGE3D_MAX_WIDTH:
		case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
		case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
		case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
		case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
		case CL_DEVICE_IMAGE3D_MAX_DEPTH:
		case CL_DEVICE_PROFILING_TIMER_RESOLUTION:
#if defined(CL_VERSION_2_0)
		case CL_DEVICE_IMAGE_PITCH_ALIGNMENT:
		case CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT:
		case CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE:
		case CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE:
#endif
		case CL_DEVICE_MAX_PARAMETER_SIZE:
		case CL_DEVICE_MAX_WORK_GROUP_SIZE: {
			std::size_t value = 0;
			std::size_t retSize = 0;
			cl_int err = clGetDeviceInfo(device, query.mData, sizeof(value), &value, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write(SimplePacket<PacketType::Payload, uint64_t>(value));
			break;
		}
		case CL_DEVICE_MAX_WORK_ITEM_SIZES: {
			std::size_t sizes[3];
			std::size_t retSize = 0;
			cl_int err = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(sizes), sizes, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			uint64_t translated[3];
			translated[0] = sizes[0];
			translated[1] = sizes[1];
			translated[2] = sizes[2];
			mStream.write(PayloadPtr<uint8_t>(translated, 3));
			break;
		}

		// Return the raw data.
		default: {
			std::size_t retSize = 0;
			clGetDeviceInfo(device, query.mData, 0, nullptr, &retSize);
			std::vector<uint8_t> data;
			data.resize(retSize);
			cl_int err = clGetDeviceInfo(device, query.mData, data.size(), data.data(), &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write(PayloadPtr<>(data.data(), retSize));
			break;
		}
	}
}

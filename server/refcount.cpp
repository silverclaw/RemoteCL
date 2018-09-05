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

#include "hints.h"
#include "packets/refcount.h"
#include "packets/packet.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;


void ServerInstance::handleRelease()
{
	Release packet = mStream.read<Release>();
	switch (packet.mObjTy) {
		case 'D':
		{
			cl_uint err = clReleaseDevice(getObj<cl_device_id>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'C':
		{
			cl_uint err = clReleaseContext(getObj<cl_context>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'Q':
		{
			cl_uint err = clReleaseCommandQueue(getObj<cl_command_queue>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'P':
		{
			cl_uint err = clReleaseProgram(getObj<cl_program>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'K':
		{
			cl_uint err = clReleaseKernel(getObj<cl_kernel>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		case 'M':
		{
			cl_uint err = clReleaseMemObject(getObj<cl_mem>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		case 'E':
		{
			cl_uint err = clReleaseEvent(getObj<cl_event>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		default:
			assert(false && "invalid object type");
	}

	mStream.write<SuccessPacket>({});
}

void ServerInstance::handleRetain()
{
	Retain packet = mStream.read<Retain>();
	switch (packet.mObjTy) {
		case 'D':
		{
			cl_uint err = clRetainDevice(getObj<cl_device_id>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'C':
		{
			cl_uint err = clRetainContext(getObj<cl_context>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		case 'Q':
		{
			cl_uint err = clRetainCommandQueue(getObj<cl_command_queue>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'P':
		{
			cl_uint err = clRetainProgram(getObj<cl_program>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'K':
		{
			cl_uint err = clRetainKernel(getObj<cl_kernel>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			break;
		}
		case 'M':
		{
			cl_uint err = clRetainMemObject(getObj<cl_mem>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		case 'E':
		{
			cl_uint err = clRetainEvent(getObj<cl_event>(packet.mID));
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(ErrorPacket (err));
				return;
			}
			break;
		}
		default:
			assert(false && "invalid object type");
	}

	mStream.write<SuccessPacket>({});
}

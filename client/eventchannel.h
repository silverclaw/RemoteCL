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

#if !defined(REMOTECL_CLIENT_EVENTCHANNEL_H)
#define REMOTECL_CLIENT_EVENTCHANNEL_H

#include "CL/cl.h"

#include <cassert>
#include <memory>

namespace RemoteCL
{
namespace Client
{

typedef void (CL_CALLBACK *CL_EVENT_CB) (cl_event event,
                                         cl_int event_command_exec_status,
                                         void *user_data);

static uint32_t CallbackID = 1;

struct CLEventCallback
{
	explicit CLEventCallback(CL_EVENT_CB cb) noexcept : ID(CallbackID), callback(cb) {
		assert(ID > 0);
		CallbackID++;
	}

	// unique callback ID
	uint32_t ID;

	cl_event event;
	cl_int callbackType;
	CL_EVENT_CB callback;
	void* userData;
};

}
}

#endif

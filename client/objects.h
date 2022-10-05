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

#if !defined(REMOTECL_CLIENT_OBJECTS_H)
#define REMOTECL_CLIENT_OBJECTS_H

#include "CL/cl_platform.h"
#include "CL/cl_icd.h"

#include "idtype.h"

#include <list>
#include <memory>
#include <mutex>

namespace RemoteCL
{
namespace Client
{
struct CLObject
{
	explicit CLObject(IDType id) noexcept : ID(id) {}
	virtual ~CLObject() {}

	const IDType ID;
};

extern cl_icd_dispatch OCLDispatchTable;

/// Wraps this internal type around the ICD dispatchable structure.
/// The ICD dispatch object contains a pointer to the dispatch table as the first
/// member and the remaining members are implementation-defined. We use that to store
/// a pointer to the internal object.
template<typename InternalType>
struct MakeDispatchable
{
	cl_icd_dispatch* dispatchTable;
	InternalType* ptr;
};

/// Mixin to make the Parent class ICD-dispatchable.
/// This should be used as CRTP. The dispatchable object declared inside
/// has its address taken and returned when the class is converted into the OpenCL external type.
/// To Unwrap, the Dispatchable object contains the original internal object pointer.
template<typename Parent, typename CLAlias>
class ICDDispatchable : public CLObject
{
public:
	typedef CLAlias OpenCLType;
	typedef MakeDispatchable<Parent> DispatchableType;

private:
	/// The OpenCL dispatchable object.
	DispatchableType mDispatchObject{&OCLDispatchTable, static_cast<Parent*>(this)};

public:
	using CLObject::CLObject;
	ICDDispatchable() noexcept {}
	ICDDispatchable(const OpenCLType clObj) noexcept : CLObject(0),
		mDispatchObject(*reinterpret_cast<const DispatchableType*>(clObj)) {}

	// Allows direct conversion to the OpenCL type.
	operator OpenCLType() noexcept { return reinterpret_cast<CLAlias>(&mDispatchObject); }
	// Used for unwrapping - retrieving the internal object from a dispatchable.
	operator Parent&() noexcept { return *mDispatchObject.ptr; }
};

struct PlatformID final : public ICDDispatchable<PlatformID, cl_platform_id>
{
	using ICDDispatchable::ICDDispatchable;
};

struct DeviceID final : public ICDDispatchable<DeviceID, cl_device_id>
{
	using ICDDispatchable::ICDDispatchable;
};

struct Context final : public ICDDispatchable<Context, cl_context>
{
	using ICDDispatchable::ICDDispatchable;
};

struct Queue final : public ICDDispatchable<Queue, cl_command_queue>
{
	using ICDDispatchable::ICDDispatchable;
};

struct Program final : public ICDDispatchable<Program, cl_program>
{
	using ICDDispatchable::ICDDispatchable;
};

struct Kernel final : public ICDDispatchable<Kernel, cl_kernel>
{
	using ICDDispatchable::ICDDispatchable;
};

class MemObject final : public ICDDispatchable<MemObject, cl_mem>
{
public:
	using ICDDispatchable::ICDDispatchable;

	/// Represents a mapping of this object.
	class Mapping
	{
	public:
		Mapping(std::size_t size, std::size_t offset, cl_map_flags flags) :
			data(new uint8_t[size]), size(size), offset(offset), flags(flags) {}

		operator void*() noexcept
		{
			return static_cast<void*>(data.get());
		}

		bool operator==(void* ptr) const noexcept
		{
			return ptr == data.get();
		}

	private:
		std::unique_ptr<uint8_t[]> data;

	public:
		const size_t size;
		const size_t offset;
		const cl_map_flags flags;
	};

	/// Retrieves a memory buffer which can be used to map this Memory Object.
	Mapping& getMapping(std::size_t size, std::size_t offset, cl_map_flags flags)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mMappings.emplace_back(size, offset, flags);
		return mMappings.back();
	}

	Mapping& getMapping(void* ptr)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		for (auto it = mMappings.begin(); it != mMappings.end(); ++it) {
			if (*it == ptr) {
				return *it;
			}
		}

		throw CL_INVALID_VALUE;
	}

	/// Releases this memory buffer.
	void dropMapping(void* ptr)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		for (auto it = mMappings.begin(); it != mMappings.end(); ++it) {
			if (*it == ptr) {
				mMappings.erase(it);
				return;
			}
		}
		// Ownership of the mapping must have been estabilished.
		throw CL_INVALID_VALUE;
	}

private:
	/// List of mappings on this memory object.
	std::list<Mapping> mMappings;
	/// Mutex used to synchronise reading/writing of mappings.
	std::mutex mMutex;

};

struct Event final : public ICDDispatchable<Event, cl_event>
{
	using ICDDispatchable::ICDDispatchable;
};

/// Extracts the internal object from the OpenCL dispatchable type.
#define REMOTECL_TYPE_UNWRAPPER(type) \
inline type& Unwrap(type::OpenCLType arg) noexcept \
{ \
	ICDDispatchable<type, type::OpenCLType> wrapped(arg); \
	return wrapped; \
}

#if defined(_MSC_VER)
#define PURE_NONNULL
#else
#define PURE_NONNULL __attribute__((pure))  __attribute__((nonnull(1)))
#endif

namespace Unwrappers
{
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(PlatformID);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(DeviceID);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(Context);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(Queue);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(Program);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(Kernel);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(MemObject);
PURE_NONNULL REMOTECL_TYPE_UNWRAPPER(Event);
}

#undef PURE_NONNULL
#undef REMOTECL_TYPE_UNWRAPPER

template<typename T>
IDType GetID(T t) noexcept
{
	return Unwrappers::Unwrap(t).ID;
}
}
}

#endif

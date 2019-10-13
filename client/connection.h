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

#if !defined(REMOTECL_CLIENT_CONNECTION_H)
#define REMOTECL_CLIENT_CONNECTION_H

#include <iostream>
#include <list>
#include <memory>
#include <vector>
#include <type_traits>
#include <mutex>
#include <memory>

#include "idtype.h"
#include "packetstream.h"

namespace RemoteCL
{
namespace Client
{
struct CLObject;
class LockedConnection;

class Callback
{
public:
	/// Trigger this callback, whatever it may do.
	/// The stream provided is the event stream, not the API stream.
	virtual void trigger(PacketStream&) noexcept = 0;
	virtual ~Callback() = default;
};

/// Describes a client connection.
/// This isn't meant to be used directly. Use get() to acquire a locked
/// access handle.
class Connection final
{
public:
	Connection() noexcept;

	/// Acquire a locked handle to use the connection.
	LockedConnection get();

	/// Checks if the callback stream is available for callback registration.
	bool hasEventStream() const noexcept
	{
		return mEventStream != nullptr;
	}

	~Connection();

private:
	friend class LockedConnection;
	/// Data stream for regular CL API communication.
	std::unique_ptr<PacketStream> mStream;
	/// Data stream for server-side requests and notifications.
	std::unique_ptr<PacketStream> mEventStream;
	/// List of registered callbacks on the connection.
	std::vector<std::unique_ptr<Callback>> mCallbacks;
	/// Serialises accesses to mCallbacks.
	std::mutex mCallbackMutex;
	/// All of the objects that have been queried by the client.
	/// Each will have a unique ID which is effectively an index into this vector.
	std::vector<std::unique_ptr<CLObject>> mObjects;
	std::mutex mMutex;
};

/// Allows access to the connection internals through an auto-locked handle.
class LockedConnection final
{
private:
	LockedConnection(const LockedConnection&) = delete;
	LockedConnection& operator=(const LockedConnection&) = delete;
	explicit LockedConnection(Connection& parent) :
		mLock(parent.mMutex), mParent(parent)
	{
		if (!parent.mStream) throw Socket::Error();
	}

	std::unique_lock<std::mutex> mLock;
	Connection& mParent;

public:
	LockedConnection(LockedConnection&&) = default;
	LockedConnection& operator=(LockedConnection&&) = default;

	template<typename ObjTy>
	ObjTy* getObject(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		// std::vector default-initialises to nullptr, which is what we want.
		return static_cast<ObjTy*>(mParent.mObjects[id].get());
	}

	template<typename ObjTy>
	ObjTy& registerID(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		std::unique_ptr<ObjTy> obj(new ObjTy(id));
		ObjTy* ptr = obj.get();
		if (mParent.mObjects.size() <= id)
			mParent.mObjects.resize(id+1);
		mParent.mObjects[id] = std::move(obj);
		return *ptr;
	}

	template<typename ObjTy>
	ObjTy& getOrInsertObject(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		if (id < mParent.mObjects.size())
			return *static_cast<ObjTy*>(mParent.mObjects[id].get());
		return registerID<ObjTy>(id);
	}

	uint32_t registerCallback(std::unique_ptr<Callback> callback)
	{
		// This doesn't need to be done under a LockedConnection,
		// but for API simplicity, leave it so.
		std::unique_lock<std::mutex> lock(mParent.mCallbackMutex);
		mParent.mCallbacks.emplace_back(std::move(callback));
		return (mParent.mCallbacks.size()-1);
	}

	/// Send and receive packets through this connection.
	PacketStream* operator->() noexcept
	{
		return mParent.mStream.get();
	}

	friend class Connection;
};

inline LockedConnection Connection::get()
{
	return LockedConnection(*this);
}

/// Each client only has one connection to one server, created at process start time.
extern Connection gConnection;

} // namespace Client
} // namespace RemoteCL

#endif

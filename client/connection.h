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
#include <memory>
#include <map>
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

/// Describes a client connection.
/// This isn't meant to be used directly. Use get() to acquire a locked
/// access handle.
class Connection final
{
public:
	Connection() noexcept;

	/// Acquire a locked handle to use the connection.
	LockedConnection get();

	~Connection();

private:
	friend class LockedConnection;
	std::unique_ptr<PacketStream> mStream;
	/// All of the objects that have been queried by the client.
	/// Each will have a unique ID which is effectively an index into this vector.
	std::map<IDType, CLObject*> mObjects;
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
		// std::map default-initialises to nullptr, which is what we want.
		return static_cast<ObjTy*>(mParent.mObjects[id]);
	}

	template<typename ObjTy>
	ObjTy& registerID(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		std::unique_ptr<ObjTy> obj(new ObjTy(id));
		ObjTy* ptr = obj.get();
		mParent.mObjects.insert({id, ptr});
		obj.release();
		return *ptr;
	}

	template<typename ObjTy>
	ObjTy& getOrInsertObject(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		auto it = mParent.mObjects.find(id);
		if (it != mParent.mObjects.end()) return *static_cast<ObjTy*>(it->second);
		return registerID<ObjTy>(id);
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

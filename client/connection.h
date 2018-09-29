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
class LockedStream final
{
public:
	LockedStream(std::unique_lock<std::mutex> mutex, PacketStream* stream) :
		mLock(std::move(mutex)), mStream(stream)
	{
	}

	LockedStream(const LockedStream&) = delete;
	LockedStream(LockedStream&&) = default;
	LockedStream& operator=(const LockedStream&) = delete;
	LockedStream& operator=(LockedStream&&) = delete;

	PacketStream* operator->() noexcept
	{
		return mStream;
	}

	// This will eventually be removed.
	// Added only during a transition period.
	PacketStream& operator*() noexcept
	{
		return *mStream;
	}

	bool operator!() const noexcept
	{
		return mStream == nullptr;
	}

	explicit operator bool() const noexcept
	{
		return mStream != nullptr;
	}

private:
	std::unique_lock<std::mutex> mLock;
	PacketStream* mStream;
};

struct CLObject;
class Connection
{
public:
	Connection() noexcept;

	LockedStream getLockedStream()
	{
		return {std::unique_lock<std::mutex>(mMutex), mStream.get()};
	}

	template<typename ObjTy>
	ObjTy* getObject(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		// std::map default-initialises to nullptr, which is what we want.
		return static_cast<ObjTy*>(mObjects[id]);
	}

	template<typename ObjTy>
	ObjTy& registerID(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		std::unique_ptr<ObjTy> obj(new ObjTy(id));
		ObjTy* ptr = obj.get();
		mObjects.insert({id, ptr});
		obj.release();
		return *ptr;
	}

	template<typename ObjTy>
	ObjTy& getOrInsertObject(IDType id)
	{
		static_assert(std::is_base_of<CLObject, ObjTy>::value, "Invalid object queried");
		auto it = mObjects.find(id);
		if (it != mObjects.end()) return *static_cast<ObjTy*>(it->second);
		return registerID<ObjTy>(id);
	}

	~Connection();

private:
	std::unique_ptr<PacketStream> mStream;
	/// All of the objects that have been queried by the client.
	/// Each will have a unique ID which is effectively an index into this vector.
	std::map<IDType, CLObject*> mObjects;
	std::mutex mMutex;
};

/// Each client only has one connection to one server, created at process start time.
extern Connection gConnection;

} // namespace Client
} // namespace RemoteCL

#endif

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

/// @file version.cpp Defines the version check functions.

#include "packets/version.h"

#include <algorithm> // std::find
#include <cstring> // std::memcmp
#include <iterator> // std::begin,end

using namespace RemoteCL;

bool VersionPacket::eventEnabled() const noexcept
{
	// Search for the 'e' in the version string.
	auto match = std::find(std::begin(mVersion)+SWVersionSize, std::end(mVersion), 'e');
	return match != std::end(mVersion);
}

bool VersionPacket::compressionEnabled() const noexcept
{
	// Search for the 'z' in the version string.
	auto match = std::find(std::begin(mVersion)+SWVersionSize, std::end(mVersion), 'z');
	return match != std::end(mVersion);
}

bool VersionPacket::isCompatibleWith(const VersionPacket& v) const noexcept
{
	// Client/server versions must match.
	// Next is the size in bytes of the type used for Object IDs, which must match.
	// In theory we could allow a different size, but we'd have to adjust the (de)serialiser
	// of any packets that use IDType. Too much hassle; just reject different sizes.
	if (std::memcmp(mVersion, v.mVersion, SWVersionSize) != 0) {
		return false;
	}

	// These features/extensions must match for a successful connection.
	if (v.compressionEnabled() != compressionEnabled()) {
		return false;
	}

	// The event stream support feature may mismatch, so we don't need to check.

	return true;
}


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

#if !defined(REMOTECL_HINTS_H)
#define REMOTECL_HINTS_H
/// @file hints.h Defines macros for compiler hints and specifications.

// SO_EXPORT - Marks the associated function as being visible from this Shared Library.
// (Un)Likely - Branch predictor hints.
// Unreachable - Marks a code-path as dead.

#if defined(_MSC_VER)
	// It would be preferable to use __declspec(dllexport) but the cl* functions
	// have already been defined with specific linkage on cl.h, so we have to use the .def file method.
	#define SO_EXPORT
	#define Unlikely(X) X
	#define Likely(X) X
	#define Unreachable() __assume(0)
	// Stop warning spammage on MSVC
	// Loss of precision for integer assignments and deprecated declarations (from CL headers).
	#pragma warning(disable : 4267 4244 4996)
#else
	#define SO_EXPORT __attribute__((visibility("protected")))
	#define Unlikely(X) __builtin_expect(!!(X), 0)
	#define Likely(X) __builtin_expect(!!(X), 1)
	#define Unreachable() __builtin_unreachable()
#endif


#endif

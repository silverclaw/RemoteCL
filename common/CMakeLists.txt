add_library(RemoteCL EXCLUDE_FROM_ALL
	socket.cpp
	socketstream.cpp)

if (WIN32)
	target_link_libraries(RemoteCL wsock32 ws2_32)
endif (WIN32)

find_package(ZLIB)
if (ZLIB_FOUND)
	option(REMOTECL_ENABLE_ZLIB "Use ZLIB to compress large payloads" ON)
	if (REMOTECL_ENABLE_ZLIB)
		target_compile_definitions(RemoteCL PUBLIC REMOTECL_USE_ZLIB=1)
		target_include_directories(RemoteCL PUBLIC ${ZLIB_INCLUDE_DIRS})
		target_link_libraries(RemoteCL ${ZLIB_LIBRARIES})
	endif()
endif (ZLIB_FOUND)

set(REMOTECL_DEFAULT_PORT 23857 CACHE STRING "The default port used by the client and server.")

# I wanted this in the project-level cmake, but add_compile_definitions requires a too-recent CMake version.
# Instead, set it here and allow it to propagate to the client and server targets.
target_compile_definitions(RemoteCL PUBLIC CL_TARGET_OPENCL_VERSION=${CL_TARGET_OPENCL_VERSION})

if (WIN32)
	# Required for winsock includes.
	target_compile_definitions(RemoteCL PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif(WIN32)
target_compile_definitions(RemoteCL PUBLIC REMOTECL_DEFAULT_PORT=${REMOTECL_DEFAULT_PORT})
target_include_directories(RemoteCL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(RemoteCL PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(RemoteCL PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
set_target_properties(RemoteCL PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(RemoteCL PROPERTIES VISIBILITY_INLINES_HIDDEN ON)

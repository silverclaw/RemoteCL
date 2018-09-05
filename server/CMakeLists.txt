# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_SOURCE_DIR ON)

add_executable(RemoteCLServer EXCLUDE_FROM_ALL
	main.cpp
	context.cpp
	device.cpp
	event.cpp
	memory.cpp
	program.cpp
	queue.cpp
	refcount.cpp
	instance.cpp)

option(REMOTECL_SERVER_USE_THREADS "Use a thread instead of fork() for the connection." OFF)

if (REMOTECL_SERVER_USE_THREADS)
	target_compile_definitions(RemoteCLServer PRIVATE REMOTECL_SERVER_USE_THREADS=1)
	# Even when using C++11 threads, we still need to link pthread in.
	if (NOT WIN32)
		target_compile_options(RemoteCLServer PRIVATE -pthread)
		target_link_libraries(RemoteCLServer RemoteCL pthread)
	endif()
endif (REMOTECL_SERVER_USE_THREADS)

if (NOT WIN32)
	# When building with support for a too-recent CL standard, there will be
	# deprecation warnings; we still want to expose the functionality, so ignore those.
	target_compile_options(RemoteCLServer PRIVATE -Wno-deprecated-declarations)
endif (NOT WIN32)
target_include_directories(RemoteCLServer SYSTEM PRIVATE ${KHR_INCLUDE_DIR})
# The CL header spams warnings, so silence them by setting it as a system include.
set(CL_LIBRARY "OpenCL" CACHE FILEPATH "The OpenCL library used by server.")
target_link_libraries(RemoteCLServer RemoteCL ${CL_LIBRARY})

set_target_properties(RemoteCLServer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)

option(ENABLE_SERVER "Build the server component" ON)
if (ENABLE_SERVER)
	set_target_properties(RemoteCLServer PROPERTIES EXCLUDE_FROM_ALL FALSE)
	install(TARGETS RemoteCLServer DESTINATION bin COMPONENT server)
	cpack_add_component(server DESCRIPTION "RemoteCL server executable.")
endif(ENABLE_SERVER)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_SOURCE_DIR ON)

add_executable(RemoteCLServer EXCLUDE_FROM_ALL
	main.cpp
	context.cpp
	device.cpp
	event.cpp
	image.cpp
	memory.cpp
	program.cpp
	queue.cpp
	refcount.cpp
	instance.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
if (Threads_FOUND)
	# MSVC does not have fork() so it is forced to use threads.
	if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		target_compile_definitions(RemoteCLServer PRIVATE REMOTECL_SERVER_USE_THREADS)
	else ()
		option(REMOTECL_SERVER_USE_THREADS "Use a thread instead of fork() for the connection." OFF)
		if (REMOTECL_SERVER_USE_THREADS)
			target_compile_definitions(RemoteCLServer PRIVATE REMOTECL_SERVER_USE_THREADS)
			# Even when using C++11 threads, we still need to link pthread in.
			target_link_libraries(RemoteCLServer PRIVATE Threads::Threads)
		endif (REMOTECL_SERVER_USE_THREADS)
	endif()
endif (Threads_FOUND)

set(CL_TARGET_OPENCL_VERSION 220 CACHE STRING "Target OpenCL version for server.")
if (${CL_TARGET_OPENCL_VERSION} LESS 120)
	message(SEND_ERROR "The minimum required OpenCL version is 120.")
endif()
target_compile_definitions(RemoteCLServer PUBLIC CL_TARGET_OPENCL_VERSION=${CL_TARGET_OPENCL_VERSION})

if (NOT WIN32)
	# When building with support for a too-recent CL standard, there will be
	# deprecation warnings; we still want to expose the functionality, so ignore those.
	target_compile_options(RemoteCLServer PRIVATE -Wno-deprecated-declarations)
endif (NOT WIN32)

find_package(OpenCL)
if (NOT OpenCL_FOUND)
	include(FetchContent)
    FetchContent_Declare(
        OpenCLICD
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(OpenCLICD)
endif()

# The CL header spams warnings, so silence them by setting it as a system include.
set(CL_LIBRARY "OpenCL" CACHE FILEPATH "The OpenCL library used by server.")
target_link_libraries(RemoteCLServer PRIVATE RemoteCL OpenCL::Headers OpenCL::OpenCL)

set_target_properties(RemoteCLServer PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)

option(ENABLE_SERVER "Build the server component" ON)
if (ENABLE_SERVER)
	set_target_properties(RemoteCLServer PROPERTIES EXCLUDE_FROM_ALL FALSE)
	install(TARGETS RemoteCLServer DESTINATION bin COMPONENT server)
	cpack_add_component(server DESCRIPTION "RemoteCL server executable.")
endif(ENABLE_SERVER)

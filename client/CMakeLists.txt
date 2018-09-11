# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_SOURCE_DIR ON)

add_library(RemoteCLClient EXCLUDE_FROM_ALL SHARED
	connection.cpp
	context.cpp
	device.cpp
	event.cpp
	icd.cpp
	image.cpp
	kernel.cpp
	memory.cpp
	platform.cpp
	program.cpp
	queue.cpp )

# This will silence deprecation warnings from the OpenCL headers.
# Required because we reference the functions from the ICD support file.
target_compile_definitions(RemoteCLClient PRIVATE
	CL_USE_DEPRECATED_OPENCL_1_0_APIS CL_USE_DEPRECATED_OPENCL_1_1_APIS CL_USE_DEPRECATED_OPENCL_1_2_APIS
	CL_USE_DEPRECATED_OPENCL_2_0_APIS CL_USE_DEPRECATED_OPENCL_2_1_APIS)

# The CL header spams warnings, so silence them by setting it as a system include.
target_include_directories(RemoteCLClient SYSTEM PRIVATE ${KHR_INCLUDE_DIR})
target_link_libraries(RemoteCLClient RemoteCL)
target_include_directories(RemoteCLClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
	target_sources(RemoteCLClient PRIVATE exports.def)
endif (WIN32)

set(CLIENT_DEFAULT_HOST "localhost" CACHE STRING "The default hostname used by the client.")
target_compile_definitions(RemoteCLClient PRIVATE DEFAULT_REMOTE_HOST="${CLIENT_DEFAULT_HOST}")

set_target_properties(RemoteCLClient PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
set_target_properties(RemoteCLClient PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(RemoteCLClient PROPERTIES VISIBILITY_INLINES_HIDDEN ON)

option(ENABLE_CLIENT "Build the client component" ON)
if (ENABLE_CLIENT)
	set_target_properties(RemoteCLClient PROPERTIES EXCLUDE_FROM_ALL FALSE)
	install(TARGETS RemoteCLClient DESTINATION lib COMPONENT client)
	cpack_add_component(client DESCRIPTION "RemoteCL client library.")
endif(ENABLE_CLIENT)

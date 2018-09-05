cmake_minimum_required(VERSION 3.3.0)
# CMP0063 - respect the visibility policy for all targets.
cmake_policy(SET CMP0063 NEW)
project(RemoteCL LANGUAGES CXX VERSION 0.1)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No build type selected, defaulting to Debug.")
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)." FORCE)
endif()

set(KHR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/khronos/" CACHE PATH "Path to the Khronos headers.")

find_program(CCACHE_BIN ccache DOC "Path to CCache binary")
if (CCACHE_BIN)
	option(USE_CCACHE "Enable use of CCache." ON)
	if (USE_CCACHE)
		set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_BIN})
	endif(USE_CCACHE)
endif(CCACHE_BIN)


set(CL_TARGET_OPENCL_VERSION 220 CACHE STRING "Target OpenCL version.")
if (${CL_TARGET_OPENCL_VERSION} LESS 120)
	message(SEND_ERROR "The minimum required OpenCL version is 120.")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenCL call forwarding.")
set(CPACK_PACKAGE_DESCRIPTION "Forwards OpenCL API calls over a socket.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/RemoteCL")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_LICENSE "LGPL")
include(CPack)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

find_package(Doxygen)
if (DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating Doxygen documentation."
        VERBATIM)
endif(DOXYGEN_FOUND)

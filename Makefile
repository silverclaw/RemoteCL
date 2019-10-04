# This is a simple Makefile to build RemoteCL.
# You should prefer to use the CMakeLists.txt instead, but on systems where CMake
# may not be readily available, this Makefile might just get you what you need.
# Please note that this file is provided merely for convenience and it may be outdated.

# Refer to the README.md for the usual configure options:
# ZLib is default enabled.
ENABLE_ZLIB ?= 1
# Where the Khronos headers are located.
KHR_INCLUDE_DIR ?= external/khronos
# Default CL version
CL_TARGET_OPENCL_VERSION ?= 220
# Default RemoteCL port.
DEFAULT_PORT ?= 23857
# Default RemoteCL client hostname.
DEFAULT_HOST ?= localhost
# Default OpenCL implementation for server linking.
CL_LIBRARY ?= OpenCL
# By default, the server will use fork()
USE_THREADS ?= 0


# Default c++ compiler to use.
CXX ?= g++
# Default optimisation level.
CXXFLAGS ?= -O2 -fPIC -DNDEBUG
# Use the GCC AR wrapper in case someone tries to -flto.
AR ?= gcc-ar

ifeq ($(ENABLE_ZLIB),1)
	CXXFLAGS += -DENABLE_ZLIB
endif

ifeq ($(USE_THREADS),1)
	CXXFLAGS += -DREMOTECL_SERVER_USE_THREADS
	CXXFLAGS += -pthread
endif

# Required CXX flags for building
CXXFLAGS += -std=c++11 -fvisibility=hidden -fvisibility-inlines-hidden -isystem $(KHR_INCLUDE_DIR) -I common/
CXXFLAGS += -DCL_TARGET_OPENCL_VERSION=$(CL_TARGET_OPENCL_VERSION)
CXXFLAGS += -DREMOTECL_DEFAULT_PORT=$(DEFAULT_PORT)
CXXFLAGS += -DDEFAULT_REMOTE_HOST="\"$(DEFAULT_HOST)\""
# These must be manually updated:
CXXFLAGS += -DREMOTECL_VERSION_MAJ=0 -DREMOTECL_VERSION_MIN=5

# Glob for all sources.
CLIENT_SRCS := $(wildcard client/*.cpp)
SERVER_SRCS := $(wildcard server/*.cpp)
COMMON_SRCS := $(wildcard common/*.cpp)

# And associated object file list.
CLIENT_OBJS := $(patsubst %.cpp,%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst %.cpp,%.o,$(SERVER_SRCS))
COMMON_OBJS := $(patsubst %.cpp,%.o,$(COMMON_SRCS))

.PHONY: all clean

all: RemoteCLServer libRemoteCLClient.so libRemoteCLClient.a

clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) $(COMMON_OBJS) libRemoteCL.a libRemoteCLClient.a libRemoteCLClient.so RemoteCLServer

libRemoteCL.a: $(COMMON_OBJS)
	$(AR) r libRemoteCL.a $(COMMON_OBJS)

libRemoteCLClient.a: $(CLIENT_OBJS)
	$(AR) r libRemoteCLClient.a $(CLIENT_OBJS)

libRemoteCLClient.so: $(CLIENT_OBJS) libRemoteCL.a
	$(CXX) $(CLIENT_OBJS) libRemoteCL.a -shared -o libRemoteCLClient.so

RemoteCLServer: $(SERVER_OBJS) libRemoteCL.a
	$(CXX) $(SERVER_OBJS) libRemoteCL.a -o RemoteCLServer $(CL_LIBRARY)

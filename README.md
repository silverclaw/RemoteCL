# RemoteCL
Allows forwarding of OpenCL API calls through a network socket.
An application can then create OpenCL objects and run kernels on another computer, possibly running a different OS.

This is useful when you want to debug an application that does not run on a target system. Or when you want easy process isolation between an application and the OpenCL implementation (ie, a buggy driver).

This isn't an actual OpenCL implementation; it serves as a bridge between an application and some other, real implementation available elsewhere.

Not all OpenCL calls are implemented; only the set of calls that I needed for my own uses. More functionality can be added when required. For a list of which OpenCL functions are currently forwarded, see `client/exports.def`.

I'm not particularly proud of the code; it works, which is what I needed. Pieces have been rewritten several times which lead to the overall design being a bit inconsistent, but hopefully it still all makes sense.
Improvement and suggestions are thus welcome.

The Windows builds have not been well tested. I've used the client library, but not the server.


## Requirements
Builds complete on Linux and Windows. I have successfully built with
* GCC 5, 6, 7, and 8 (also cross-compiled to ARM, ARM64, and MIPS64)
* Clang 5, 6, 7, and 8
* MSVC 19.14

All you need is a C++11 capable compiler.

The server and clients can be built separately.

The build will attempt to link the server binary with a libOpenCL.so; thus, if you're cross-compiling, you'll need that available.
You won't need to fetch any further dependencies, but you'll obviously need a network sockets implementation which should be available on your system already.

ZLib is an optional dependency, If enabled, large data transfers are compressed before being sent over the network.


## Usage
Start the server binary somewhere. By default, port 23857 is used (which I picked randomly). You can change that by adding `--port 23857` to your server start command or rebuilding with the appropriate CMake option (`REMOTECL_DEFAULT_PORT`).

Rename or symlink libRemoteCL.so to whatever library name your application is linking against (normally, this wil be libOpenCL.so). Use the `LD_LIBRARY_PATH=...` to have your system load the RemoteCL library. Alternatively, build your application and link against libRemoteCL.so.
Run your application with the environment variable `REMOTECL="host=xxx.xxx.xxx.xxx;port=xxxxx"` where "host" is the IP address (or hostname) of the machine running the server, and "port" whatever port you configured. If you don't specify a port, the default will be used.

The default host-name can be configured at compile-time with CMake `CLIENT_DEFAULT_HOST`; it is preset to "localhost".

If, for some reason, the connection is dropped or cut, the OpenCL calls will start returning `CL_DEVICE_NOT_AVAILABLE`. The client will not reconnect - once the connection drops, that's it.
It should be possible to make the client reconnect, but any OpenCL Objects would be invalid.

Using an unsupported feature will cause the CL call to return `CL_INVALID_OPERATION` (see Known Limitations).


## Building
Standard CMake. There are no other dependencies, as the required [Khronos](https://www.khronos.org/) headers are in here (but you can use your own if you'd like). You can pick which Khronos headers to build against using the `KHR_INCLUDE_DIR` CMake option. By default, the build will use the shipped headers (see `external/README.md`).

Under CMake, you can pick if you want to build the server, or the client, or both. Thus, if you're cross compiling, you only need to build only one of them.

You can (should) set the OpenCL version you want to support (or the one supported by your platform) by setting `CL_TARGET_OPENCL_VERSION` (which defaults to 220). Attempting to support a version above the one your platform exposes might lead to link errors or runtime crashes.

The server option `REMOTECL_SERVER_USE_THREADS` (default off) will make the server spawn a thread instead of forking the parent process when a connection is accepted. This decreases security as there is no memory separation as well as possibly being unsafe if one of the connections causes a crash (for example, on a wild pointer). It does, however, make it easier to debug without having to set up follow-child process.

The option `REMOTECL_ENABLE_ZLIB` will make large data packets be zlib compressed before being sent through the network. The minimum size of the packet to be compressed is set in-source (see `packet/payload.h`). You may want to disable this if your cross-compile setup does not have zlib or if you're on a fast local network, where compressing would just waste time. Note that a server and client with different zlib configurations will not connect.

The option `REMOTECL_ENABLE_ASYNC` enables the server-initiated packet stream socket. When set to `OFF`, the server is purely reactive to client-side requests and cannot notify the client of changes in the server status. When this option is enabled (the default), then the server can trigger events. This is required for OpenCL event callbacks, which will not be supported if this option is disabled (requests return CL_UNSUPPORTED_OPERATION). The server will attempt to open ports at random (for listening) and when it finds such a port, it gets the client to connect to it. If no port is available, features required by this socket will not be supported, but the other features will still function normally.
This option is not protocol-breaking, and server/clients can connect when the support option mismatches.
Enabling this option adds a dependency to a thread support library (C++11 threads).


### Cross-compiling
Nothing special, use standard CMake cross-compilation.

Note that to build a cross-compiled Server you'll need the target OpenCL implementation library available for linking. You may need to set `CMAKE_EXE_LINKER_FLAGS` to `-L/some/path` and change the `CL_LIBRARY` if necessary, depending on your cross-compilation setup.

### Cross-platform requirements
The Server uses either C++11 threads or fork(). Obviously, the default configuration of using fork will not be cross-platform.
To build on platforms without fork(), one can build the RemoteCL server using threads instead. If spawning a subprocess is a requirement of your use-case, I suggest integrating [Reproc](https://github.com/DaanDeMeyer/reproc) into RemoteCL.


## Testing
I wrote this project with some specific uses cases in mind. Because those use cases are subject to copyright, I can't add them to this project.


## Known Limitations
Obviously, the server won't have access to the client's filesystem. Thus, if you try to compile an OpenCL program that has a "#include" directive, that'll fail.

Kernel printf will print server-side, but I it should be possible to redirect stdout to the event packet stream.

I've used this between Aarch64 and X86-64, and it seemed to work.

Only IPv4 is supported. I haven't bothered doing IPv6 because I never needed it.

Data sent over the wire is not encrypted (no TLS). If you're transferring sensitive data, you'll have to come up with some other way to secure your connection.


## Licence
This project is made available under the [GNU LGPL](https://www.gnu.org/licenses/lgpl-3.0.en.html), in hopes that it'll be useful to others. Obviously, I give no warranties or guarantees.
See LICENCE.TXT in the project's root for details.


## Legal
OpenCL and the OpenCL logo are trademarks of Apple Inc.

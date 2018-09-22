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

#include "instance.h"

#include "CL/cl.h"

#include "packets/refcount.h"
#include "packets/program.h"
#include "packets/IDs.h"
#include "packets/payload.h"

#include "hints.h"

using namespace RemoteCL;
using namespace RemoteCL::Server;

void ServerInstance::buildProgram()
{
	BuildProgram build = mStream.read<BuildProgram>();
	IDListPacket idspc = mStream.read<IDListPacket>();

	std::vector<cl_device_id> ids;
	ids.reserve(idspc.mIDs.size());
	for (auto ID : idspc.mIDs) ids.push_back(getObj<cl_device_id>(ID));
	cl_program program = getObj<cl_program>(build.mID);
	if (build.mString.find("-cl-kernel-arg-info") == std::string::npos) {
		// We need this in order for clSetKernelArg to work.
		// Some compilers don't like this being specified twice, so ensure we don't
		// clone the option so that spurious diagnostics aren't generated.
		build.mString += " -cl-kernel-arg-info";
	}
	cl_int err = clBuildProgram(program, ids.size(), ids.data(), build.mString.c_str(), nullptr, nullptr);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

void ServerInstance::createProgramFromSource()
{
	ProgramSource source = mStream.read<ProgramSource>();
	cl_context context = getObj<cl_context>(source.mID);
	const cl_uint lineCount = 1;
	const char* strings[] = {source.mString.data()};
	const std::size_t lineSizes[] = {source.mString.size()};
	cl_int errCode = CL_SUCCESS;
	cl_program P = clCreateProgramWithSource(context, lineCount, strings, lineSizes, &errCode);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
	} else {
		mStream.write<IDPacket>(getIDFor(P));
	}
}

void ServerInstance::createProgramFromBinary()
{
	BinaryProgram packet = mStream.read<BinaryProgram>();
	cl_context context = getObj<cl_context>(packet.mData);

	IDListPacket listPacket = mStream.read<IDListPacket>();
	std::vector<cl_device_id> deviceList;
	deviceList.reserve(listPacket.mIDs.size());
	for (IDType id : listPacket.mIDs) {
		deviceList.push_back(getObj<cl_device_id>(id));
	}

	// There will be several payloads for each device binary. We put them all on the same buffer.
	std::vector<std::vector<uint8_t>> binaries;
	std::vector<std::size_t> binarySizes;
	binaries.reserve(deviceList.size());
	binarySizes.reserve(deviceList.size());
	for (unsigned bin = 0; bin < deviceList.size(); ++bin) {
		Payload<> binary = mStream.read<Payload<>>();
		binarySizes.push_back(binary.mData.size());
		binaries.emplace_back(std::move(binary.mData));
	}
	std::vector<const unsigned char*> binaryPtrs;
	binaryPtrs.reserve(binaries.size());
	for (auto& binary : binaries) binaryPtrs.push_back(binary.data());

	std::vector<cl_int> status;
	status.resize(binaries.size());

	cl_int errCode = CL_SUCCESS;
	cl_program program = clCreateProgramWithBinary(context, deviceList.size(), deviceList.data(),
	                                               binarySizes.data(), binaryPtrs.data(), status.data(), &errCode);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}
	mStream.write<IDPacket>(getIDFor(program));
	mStream.write<PayloadPtr<uint16_t>>({status.data(), status.size()*sizeof(cl_int)});
}

void ServerInstance::getProgramBuildInfo()
{
	ProgramBuildInfo request = mStream.read<ProgramBuildInfo>();
	cl_program program = getObj<cl_program>(request.mProgramID);
	cl_device_id device = getObj<cl_device_id>(request.mDeviceID);
	cl_program_build_info param = request.mParam;

	Payload<> reply;
	reply.mData.resize(1024);
	std::size_t replySize = 0;

	cl_int err = clGetProgramBuildInfo(program, device, param, reply.mData.size(),
	                                   reply.mData.data(), &replySize);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
		return;
	}
	if (Unlikely(replySize > reply.mData.size())) {
		err = clGetProgramBuildInfo(program, device, param, reply.mData.size(),
		                            reply.mData.data(), &replySize);
		assert(err == CL_SUCCESS);
	}
	reply.mData.resize(replySize);
	mStream.write(reply);
}

void ServerInstance::getProgramInfo()
{
	ProgramInfo info = mStream.read<ProgramInfo>();
	cl_program program = getObj<cl_program>(info.mID);
	cl_program_info param = info.mData;
	std::size_t retSize = 0;
	cl_int errCode = clGetProgramInfo(program, param, 0, nullptr, &retSize);
	if (Unlikely(errCode != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(errCode);
		return;
	}

	if (param == CL_PROGRAM_CONTEXT) {
		cl_context context = nullptr;
		assert(retSize == sizeof(context));
		clGetProgramInfo(program, param, sizeof(cl_context), &context, &retSize);
		mStream.write<IDPacket>(getIDFor(context));
	} else if (param == CL_PROGRAM_DEVICES) {
		std::vector<cl_device_id> deviceList;
		assert(retSize % sizeof(cl_device_id) == 0);
		deviceList.resize(retSize / sizeof(cl_device_id));
		errCode = clGetProgramInfo(program, param, retSize,
		                           deviceList.data(), &retSize);
		IDListPacket list;
		list.mIDs.reserve(deviceList.size());
		for (cl_device_id id : deviceList) list.mIDs.push_back(getIDFor(id));
		mStream.write(list);
	} else if (param == CL_PROGRAM_BINARIES) {
		// In this case, the API expects several buffers, one for each binary output.
		// First, fetch the size of each binary.
		std::vector<std::size_t> sizes;
		sizes.resize(retSize / sizeof(void*));
		errCode = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES,
		                           sizes.size()*sizeof(std::size_t), sizes.data(), &retSize);
		if (Unlikely(errCode != CL_SUCCESS)) {
			mStream.write<ErrorPacket>(errCode);
			return;
		}
		// Allocate memory for all binaries.
		std::vector<Payload<uint16_t>> binaries;
		std::vector<uint8_t*> ptrs;
		binaries.resize(sizes.size());
		ptrs.resize(sizes.size());
		for (std::size_t i = 0; i < binaries.size(); ++i) {
			binaries[i].mData.resize(sizes[i]);
			ptrs[i] = binaries[i].mData.data();
		}
		errCode = clGetProgramInfo(program, CL_PROGRAM_BINARIES,
		                           ptrs.size()*sizeof(char*), ptrs.data(), &retSize);

		// Write out how many binaries we're sending
		mStream.write<SimplePacket<PacketType::Payload, uint8_t>>(sizes.size());
		// Write each binary
		for (const auto& binary : binaries) mStream.write(binary);
	} else {
		Payload<> reply;
		reply.mData.resize(retSize);

		cl_int err = clGetProgramInfo(program, param, reply.mData.size(),
		                              reply.mData.data(), &retSize);
		if (Unlikely(err != CL_SUCCESS)) {
			mStream.write<ErrorPacket>(err);
			return;
		}
		mStream.write(reply);
	}
}

void ServerInstance::createKernel()
{
	KernelName name = mStream.read<KernelName>();

	cl_program program = getObj<cl_program>(name.mID);
	cl_int err = CL_SUCCESS;
	cl_kernel kernel = clCreateKernel(program, name.mString.c_str(), &err);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<IDPacket>(getIDFor(kernel));
	}
}

void ServerInstance::cloneKernel()
{
#if (CL_TARGET_OPENCL_VERSION >= 210)
	auto ID = mStream.read<SimplePacket<PacketType::CloneKernel, IDType>>();
	cl_kernel kernel = getObj<cl_kernel>(ID);
	cl_int err = CL_SUCCESS;
	cl_kernel clone = clCloneKernel(kernel, &err);
	if (Unlikely(err != CL_SUCCESS)) {
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<IDPacket>(getIDFor(clone));
	}
#else
	mStream.write<ErrorPacket>(CL_INVALID_OPERATION);
#endif
}

void ServerInstance::setKernelArg()
{
	KernelArg arg = mStream.read<KernelArg>();
	cl_uint argIndex = arg.mArgIndex;
	cl_kernel kernel = getObj<cl_kernel>(arg.mKernelID);

	// Try to figure out the type of the kernel argument.
	cl_kernel_arg_address_qualifier AS;
	std::size_t retSize = 0;
	cl_int err = clGetKernelArgInfo(kernel, argIndex, CL_KERNEL_ARG_ADDRESS_QUALIFIER, sizeof(AS), &AS, &retSize);
	if (Unlikely(err != CL_SUCCESS)) {
		// Probably the index is out of range.
		mStream.write<ErrorPacket>(err);
		return;
	}

	// Depending on the address space, ask the client to either send an ID (if this is a buffer or image)
	// or send the payload of the underlying data (might be a constant or size of local memory).
	if (AS == CL_KERNEL_ARG_ADDRESS_GLOBAL || AS == CL_KERNEL_ARG_ADDRESS_CONSTANT) {
		// Ask for an ID.
		mStream.write<SimplePacket<PacketType::Payload, char>>('I');
		mStream.flush();
		const IDType argID = mStream.read<IDPacket>();
		cl_mem memObj = getObj<cl_mem>(argID);
		// Retry the set kernel arg command.
		err = clSetKernelArg(kernel, argIndex, sizeof(cl_mem), &memObj);
	} else if (AS == CL_KERNEL_ARG_ADDRESS_LOCAL) {
		// Ask for a size for the local memory buffer.
		mStream.write<SimplePacket<PacketType::Payload, char>>('S');
		mStream.flush();
		uint32_t size = mStream.read<SimplePacket<PacketType::Payload, uint32_t>>();
		err = clSetKernelArg(kernel, argIndex, size, nullptr);
	} else {
		// Ask for a payload.
		mStream.write<SimplePacket<PacketType::Payload, char>>('P');
		mStream.flush();
		Payload<> payload = mStream.read<Payload<>>();
		// Retry the set kernel arg command.
		err = clSetKernelArg(kernel, argIndex, payload.mData.size(), payload.mData.data());
	}

	if (Unlikely(err != CL_SUCCESS)) {
		// Probably the index is out of range.
		mStream.write<ErrorPacket>(err);
	} else {
		mStream.write<SuccessPacket>({});
	}
}

void ServerInstance::getKernelInfo()
{
	KernelInfo query = mStream.read<KernelInfo>();
	cl_kernel kernel = getObj<cl_kernel>(query.mID);

	switch (query.mData) {
		case CL_KERNEL_CONTEXT: {
			cl_context context;
			std::size_t sizeRet = 0;
			cl_int err = clGetKernelInfo(kernel, CL_KERNEL_CONTEXT, sizeof(context), &context, &sizeRet);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>(getIDFor(context));
			break;
		}
		case CL_KERNEL_PROGRAM: {
			cl_program program;
			std::size_t sizeRet = 0;
			cl_int err = clGetKernelInfo(kernel, CL_KERNEL_PROGRAM, sizeof(program), &program, &sizeRet);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<IDPacket>(getIDFor(program));
			break;
		}

		case CL_KERNEL_REFERENCE_COUNT:
		case CL_KERNEL_NUM_ARGS:{
			cl_uint value;
			std::size_t sizeRet = 0;
			cl_int err = clGetKernelInfo(kernel, query.mData, sizeof(value), &value, &sizeRet);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write(SimplePacket<PacketType::Payload, uint32_t>(value));
			break;
		}

		// case CL_KERNEL_ATTRIBUTES:
		// case CL_KERNEL_FUNCTION_NAME:
		default: {
			std::size_t retSize = 0;
			clGetKernelInfo(kernel, query.mData, 0, nullptr, &retSize);
			std::vector<uint8_t> data;
			data.resize(retSize);
			cl_int err = clGetKernelInfo(kernel, query.mData, data.size(), data.data(), &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<PayloadPtr<>>({data.data(), retSize});
			break;
		}
	}
}

void ServerInstance::getKernelArgInfo()
{
	KernelArgInfo query = mStream.read<KernelArgInfo>();
	cl_kernel kernel = getObj<cl_kernel>(query.mKernelID);

	switch (query.mParam) {
		case CL_KERNEL_ARG_ACCESS_QUALIFIER:
		case CL_KERNEL_ARG_TYPE_QUALIFIER:
		case CL_KERNEL_ARG_ADDRESS_QUALIFIER: {
			cl_uint value;
			std::size_t sizeRet = 0;
			cl_int err = clGetKernelArgInfo(kernel, query.mArgIndex, query.mParam,
			                                sizeof(value), &value, &sizeRet);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<SimplePacket<PacketType::Payload, uint32_t>>(value);
			break;
		}

		//case CL_KERNEL_ARG_TYPE_NAME:
		//case CL_KERNEL_ARG_NAME:
		default: {
			std::size_t retSize = 0;
			clGetKernelArgInfo(kernel, query.mArgIndex, query.mParam, 0, nullptr, &retSize);
			std::vector<uint8_t> data;
			data.resize(retSize);
			cl_int err = clGetKernelArgInfo(kernel, query.mArgIndex, query.mParam,
			                                data.size(), data.data(), &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<PayloadPtr<>>({data.data(), retSize});
			break;
		}
	}
}

void ServerInstance::getKernelWGInfo()
{
	KernelWGInfo query = mStream.read<KernelWGInfo>();
	cl_device_id device = getObj<cl_device_id>(query.mDeviceID);
	cl_kernel kernel = getObj<cl_kernel>(query.mKernelID);

	switch (query.mParam) {
		case CL_KERNEL_COMPILE_WORK_GROUP_SIZE: {
			std::size_t sizes[3];
			std::size_t retSize = 0;
			cl_int err = clGetKernelWorkGroupInfo(kernel, device, CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
													sizeof(sizes), sizes, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			uint64_t translated[3];
			translated[0] = sizes[0];
			translated[1] = sizes[1];
			translated[2] = sizes[2];
			mStream.write(PayloadPtr<uint8_t>(translated, 3));
			break;
		}

		case CL_KERNEL_LOCAL_MEM_SIZE:
		case CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
		case CL_KERNEL_WORK_GROUP_SIZE: {
			std::size_t value = 0;
			std::size_t retSize = 0;
			cl_int err = clGetKernelWorkGroupInfo(kernel, device, query.mParam, sizeof(value), &value, &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<SimplePacket<PacketType::Payload, uint64_t>>(value);
			break;
		}

		default: {
			std::size_t retSize = 0;
			clGetKernelWorkGroupInfo(kernel, device, query.mParam, 0, nullptr, &retSize);
			std::vector<uint8_t> data;
			data.resize(retSize);
			cl_int err = clGetKernelWorkGroupInfo(kernel, device, query.mParam,
			                                      data.size(), data.data(), &retSize);
			if (Unlikely(err != CL_SUCCESS)) {
				mStream.write<ErrorPacket>(err);
				return;
			}
			mStream.write<PayloadPtr<>>({data.data(), retSize});
			break;
		}
	}
}

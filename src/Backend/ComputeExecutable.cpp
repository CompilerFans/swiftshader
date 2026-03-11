#include "ComputeExecutable.hpp"

#include "Pipeline/SemanticIRBuilder.hpp"
#include "Pipeline/SpirvBinary.hpp"

#include <spirv/unified1/spirv.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace backend {
namespace {

constexpr char kEmptyKernelSource[] = R"(extern "C" __global__ void kernel_main()
{
}
)";

constexpr char kBufferMemcpyKernelSource[] = R"(struct CsParams
{
	const unsigned int *inputData;
	unsigned int *outputData;
	unsigned int elementCount;
	unsigned int baseGroupX;
	unsigned int baseGroupY;
	unsigned int baseGroupZ;
};

extern "C" __global__ void kernel_main(CsParams params)
{
	unsigned int globalX = (params.baseGroupX + static_cast<unsigned int>(blockIdx.x)) * static_cast<unsigned int>(blockDim.x) +
	                       static_cast<unsigned int>(threadIdx.x);
	if(globalX >= params.elementCount)
	{
		return;
	}

	params.outputData[globalX] = params.inputData[globalX];
}
)";

ComputeExecutable::Kind classifyExecutableKind(const sw::SpirvBinary &spirv)
{
	if(spirv.size() < 5)
	{
		return ComputeExecutable::Kind::Unsupported;
	}

	bool hasMemoryOps = false;
	bool hasControlFlow = false;

	for(size_t idx = 5; idx < spirv.size();)
	{
		const uint32_t word = spirv[idx];
		const uint16_t wordCount = static_cast<uint16_t>(word >> 16);
		const uint16_t opcode = static_cast<uint16_t>(word & 0xFFFF);
		if(wordCount == 0 || idx + wordCount > spirv.size())
		{
			return ComputeExecutable::Kind::Unsupported;
		}

		switch(static_cast<spv::Op>(opcode))
		{
		case spv::OpAccessChain:
		case spv::OpLoad:
		case spv::OpStore:
			hasMemoryOps = true;
			break;

		case spv::OpBranch:
		case spv::OpBranchConditional:
		case spv::OpSwitch:
		case spv::OpLoopMerge:
		case spv::OpSelectionMerge:
		case spv::OpPhi:
		case spv::OpFunctionCall:
			hasControlFlow = true;
			break;

		default:
			break;
		}

		idx += wordCount;
	}

	if(!hasMemoryOps)
	{
		return ComputeExecutable::Kind::Empty;
	}

	if(!hasControlFlow)
	{
		return ComputeExecutable::Kind::BufferMemcpy;
	}

	return ComputeExecutable::Kind::Unsupported;
}

struct BufferMemcpyParams
{
	const uint32_t *inputData = nullptr;
	uint32_t *outputData = nullptr;
	uint32_t elementCount = 0;
	uint32_t baseGroupX = 0;
	uint32_t baseGroupY = 0;
	uint32_t baseGroupZ = 0;
};

}  // namespace

std::shared_ptr<ComputeExecutable> ComputeExecutable::create(const sw::ParsedSpirvInfo &parsed, const sw::SpirvBinary &spirv)
{
	if(parsed.stage != VK_SHADER_STAGE_COMPUTE_BIT)
	{
		return nullptr;
	}

	const auto kind = classifyExecutableKind(spirv);
	switch(kind)
	{
	case Kind::Empty:
		return std::shared_ptr<ComputeExecutable>(new ComputeExecutable(kind, kEmptyKernelSource));
	case Kind::BufferMemcpy:
		return std::shared_ptr<ComputeExecutable>(new ComputeExecutable(kind, kBufferMemcpyKernelSource));
	case Kind::Unsupported:
	default:
		return std::shared_ptr<ComputeExecutable>(new ComputeExecutable(Kind::Unsupported, kEmptyKernelSource));
	}
}

ModuleHandle ComputeExecutable::ensureModule(RuntimeAPI &runtime)
{
	if(!module.valid())
	{
		module = runtime.createModule(source);
	}
	return module;
}

void ComputeExecutable::dispatch(RuntimeAPI &runtime, const ComputeDispatchInfo &dispatchInfo)
{
	LaunchRecord record = {};
	record.baseGroupX = dispatchInfo.baseGroupX;
	record.baseGroupY = dispatchInfo.baseGroupY;
	record.baseGroupZ = dispatchInfo.baseGroupZ;
	record.groupCountX = dispatchInfo.groupCountX;
	record.groupCountY = dispatchInfo.groupCountY;
	record.groupCountZ = dispatchInfo.groupCountZ;
	record.blockCountX = dispatchInfo.blockCountX;
	record.blockCountY = dispatchInfo.blockCountY;
	record.blockCountZ = dispatchInfo.blockCountZ;
	record.bindingCount = dispatchInfo.bindingCount;
	record.argumentWords = dispatchInfo.argumentWords;

	if(executableKind != Kind::BufferMemcpy || !runtime.isHardwareBacked())
	{
		runtime.launch(ensureModule(runtime), record, {});
		return;
	}

	if(dispatchInfo.inputBuffer == nullptr || dispatchInfo.outputBuffer == nullptr)
	{
		return;
	}

	size_t dispatchBytes = std::min(dispatchInfo.inputSizeInBytes, dispatchInfo.outputSizeInBytes);
	dispatchBytes &= ~size_t(3);
	if(dispatchBytes == 0)
	{
		return;
	}

	auto inputMemory = runtime.allocateMemory(dispatchBytes);
	auto outputMemory = runtime.allocateMemory(dispatchBytes);
	if(!inputMemory.valid() || !outputMemory.valid())
	{
		if(outputMemory.valid())
		{
			runtime.freeMemory(outputMemory);
		}
		if(inputMemory.valid())
		{
			runtime.freeMemory(inputMemory);
		}
		return;
	}

	runtime.copyHostToMemory(inputMemory, dispatchInfo.inputBuffer, dispatchBytes);

	BufferMemcpyParams params = {};
	params.inputData = reinterpret_cast<const uint32_t *>(static_cast<uintptr_t>(runtime.memoryAddress(inputMemory)));
	params.outputData = reinterpret_cast<uint32_t *>(static_cast<uintptr_t>(runtime.memoryAddress(outputMemory)));
	params.elementCount = static_cast<uint32_t>(dispatchBytes / sizeof(uint32_t));
	params.baseGroupX = dispatchInfo.baseGroupX;
	params.baseGroupY = dispatchInfo.baseGroupY;
	params.baseGroupZ = dispatchInfo.baseGroupZ;
	std::vector<void *> arguments = { &params };
	record.argumentCount = arguments.size();

	runtime.launch(ensureModule(runtime), record, arguments);
	runtime.synchronize();

	runtime.copyMemoryToHost(dispatchInfo.outputBuffer, outputMemory, dispatchBytes);
	runtime.freeMemory(outputMemory);
	runtime.freeMemory(inputMemory);
}

}  // namespace backend

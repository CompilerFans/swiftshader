#include "FragmentBootstrap.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

namespace backend {
namespace {

struct BootstrapFsParams
{
	const FragmentBootstrapInvocation *invocations = nullptr;
	uint8_t *colorBuffer = nullptr;
	uint32_t invocationCount = 0;
	uint32_t width = 0;
	uint32_t height = 0;
};

std::string literalFloat(float value)
{
	std::ostringstream stream;
	stream.precision(std::numeric_limits<float>::max_digits10);
	stream << value;

	std::string text = stream.str();
	if(text.find('.') == std::string::npos && text.find('e') == std::string::npos && text.find('E') == std::string::npos)
	{
		text += ".0";
	}
	text += 'f';
	return text;
}

}  // namespace

std::string fragmentBootstrapCudaSource(const FragmentBootstrapConfig &config)
{
	std::ostringstream source;
	source << "struct FragmentInvocation\n"
	          "{\n"
	          "\tunsigned int x;\n"
	          "\tunsigned int y;\n"
	          "\tunsigned int exportMask;\n"
	          "\tunsigned int helperInvocation;\n"
	          "};\n\n"
	          "struct FsParams\n"
	          "{\n"
	          "\tconst FragmentInvocation *invocations;\n"
	          "\tunsigned char *colorBuffer;\n"
	          "\tunsigned int invocationCount;\n"
	          "\tunsigned int width;\n"
	          "\tunsigned int height;\n"
	          "};\n\n"
	          "static __device__ unsigned char packColor(float value)\n"
	          "{\n"
	          "\tvalue = value < 0.0f ? 0.0f : value;\n"
	          "\tvalue = value > 1.0f ? 1.0f : value;\n"
	          "\treturn static_cast<unsigned char>(value * 255.0f + 0.5f);\n"
	          "}\n\n"
	          "static __device__ void fs_main(FsParams params, unsigned int invocationIndex, const FragmentInvocation &invocation, unsigned char &outR, unsigned char &outG, unsigned char &outB, unsigned char &outA)\n"
	          "{\n"
	          "\t(void)params;\n"
	          "\t(void)invocationIndex;\n"
	       << "\toutR = packColor(" << literalFloat(config.colorR) << ");\n"
	       << "\toutG = packColor(" << literalFloat(config.colorG) << ");\n"
	       << "\toutB = packColor(" << literalFloat(config.colorB) << ");\n"
	       << "\toutA = packColor(" << literalFloat(config.colorA) << ");\n"
	          "}\n\n"
	          "extern \"C\" __global__ void fs_entry(FsParams params)\n"
	          "{\n"
	          "\tunsigned int invocationIndex = blockIdx.x * blockDim.x + threadIdx.x;\n"
	          "\tif(invocationIndex >= params.invocationCount)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tFragmentInvocation invocation = params.invocations[invocationIndex];\n"
	          "\tif(invocation.exportMask == 0u || invocation.helperInvocation != 0u)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n"
	          "\tif(invocation.x >= params.width || invocation.y >= params.height)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tunsigned char outR = 0;\n"
	          "\tunsigned char outG = 0;\n"
	          "\tunsigned char outB = 0;\n"
	          "\tunsigned char outA = 0;\n"
	          "\tfs_main(params, invocationIndex, invocation, outR, outG, outB, outA);\n"
	          "\tunsigned int offset = (invocation.y * params.width + invocation.x) * 4u;\n"
	          "\tparams.colorBuffer[offset + 0] = outR;\n"
	          "\tparams.colorBuffer[offset + 1] = outG;\n"
	          "\tparams.colorBuffer[offset + 2] = outB;\n"
	          "\tparams.colorBuffer[offset + 3] = outA;\n"
	          "}\n";
	return source.str();
}

bool runFragmentBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, const std::vector<FragmentBootstrapInvocation> &invocations, const FragmentBootstrapConfig &config, std::vector<uint8_t> *colorBuffer)
{
	if(width == 0 || height == 0 || invocations.empty())
	{
		return false;
	}

	auto module = runtime.createModule(fragmentBootstrapCudaSource(config), "fs_entry");
	if(!module.valid())
	{
		return false;
	}

	size_t invocationBytes = sizeof(FragmentBootstrapInvocation) * invocations.size();
	size_t colorBytes = static_cast<size_t>(width) * height * 4u;

	auto invocationMemory = runtime.allocateMemory(invocationBytes);
	auto colorMemory = runtime.allocateMemory(colorBytes);
	if(!invocationMemory.valid() || !colorMemory.valid())
	{
		if(colorMemory.valid())
		{
			runtime.freeMemory(colorMemory);
		}
		if(invocationMemory.valid())
		{
			runtime.freeMemory(invocationMemory);
		}
		return false;
	}

	std::vector<uint8_t> zeroColor(colorBytes, 0);
	runtime.copyHostToMemory(invocationMemory, invocations.data(), invocationBytes);
	runtime.copyHostToMemory(colorMemory, zeroColor.data(), zeroColor.size());

	BootstrapFsParams params = {};
	params.invocations = reinterpret_cast<const FragmentBootstrapInvocation *>(static_cast<uintptr_t>(runtime.memoryAddress(invocationMemory)));
	params.colorBuffer = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(runtime.memoryAddress(colorMemory)));
	params.invocationCount = static_cast<uint32_t>(invocations.size());
	params.width = width;
	params.height = height;
	std::vector<void *> arguments = { &params };

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = static_cast<uint32_t>(invocations.size());
	record.blockCountY = 1;
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();
	runtime.launch(module, record, arguments);
	runtime.synchronize();

	if(colorBuffer)
	{
		colorBuffer->resize(colorBytes);
		runtime.copyMemoryToHost(colorBuffer->data(), colorMemory, colorBytes);
	}

	runtime.freeMemory(colorMemory);
	runtime.freeMemory(invocationMemory);
	return true;
}

void launchFragmentBootstrap(RuntimeAPI &runtime)
{
	static const std::vector<FragmentBootstrapInvocation> kBootstrapInvocations = {
		{ 0u, 0u, 1u, 0u },
		{ 1u, 0u, 1u, 0u },
		{ 0u, 1u, 1u, 0u },
		{ 1u, 1u, 1u, 0u },
	};

	runFragmentBootstrap(runtime, 2u, 2u, kBootstrapInvocations, FragmentBootstrapConfig{}, nullptr);
}

}  // namespace backend

#include "FragmentBootstrap.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

namespace backend {
namespace {

constexpr uint32_t kFragmentBlockWidth = 128u;

struct BootstrapFsParams
{
	const FragmentBootstrapInvocation *invocations = nullptr;
	uint8_t *colorBuffer = nullptr;
	uint32_t invocationCount = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	float colorR = 1.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 1.0f;
	float backColorR = 0.0f;
	float backColorG = 0.0f;
	float backColorB = 1.0f;
	float backColorA = 1.0f;
	float vertexColor0R = 1.0f;
	float vertexColor0G = 1.0f;
	float vertexColor0B = 1.0f;
	float vertexColor0A = 1.0f;
	float vertexColor1R = 1.0f;
	float vertexColor1G = 1.0f;
	float vertexColor1B = 1.0f;
	float vertexColor1A = 1.0f;
	float vertexColor2R = 1.0f;
	float vertexColor2G = 1.0f;
	float vertexColor2B = 1.0f;
	float vertexColor2A = 1.0f;
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
	          "\tunsigned int frontFacing;\n"
	          "\tfloat barycentric0;\n"
	          "\tfloat barycentric1;\n"
	          "\tfloat barycentric2;\n"
	          "};\n\n"
	          "struct FsParams\n"
	          "{\n"
	          "\tconst FragmentInvocation *invocations;\n"
	          "\tunsigned char *colorBuffer;\n"
	          "\tunsigned int invocationCount;\n"
	          "\tunsigned int width;\n"
	          "\tunsigned int height;\n"
	          "\tfloat colorR;\n"
	          "\tfloat colorG;\n"
	          "\tfloat colorB;\n"
	          "\tfloat colorA;\n"
	          "\tfloat backColorR;\n"
	          "\tfloat backColorG;\n"
	          "\tfloat backColorB;\n"
	          "\tfloat backColorA;\n"
	          "\tfloat vertexColor0R;\n"
	          "\tfloat vertexColor0G;\n"
	          "\tfloat vertexColor0B;\n"
	          "\tfloat vertexColor0A;\n"
	          "\tfloat vertexColor1R;\n"
	          "\tfloat vertexColor1G;\n"
	          "\tfloat vertexColor1B;\n"
	          "\tfloat vertexColor1A;\n"
	          "\tfloat vertexColor2R;\n"
	          "\tfloat vertexColor2G;\n"
	          "\tfloat vertexColor2B;\n"
	          "\tfloat vertexColor2A;\n"
	          "};\n\n"
	          "static __device__ unsigned char packColor(float value)\n"
	          "{\n"
	          "\tvalue = value < 0.0f ? 0.0f : value;\n"
	          "\tvalue = value > 1.0f ? 1.0f : value;\n"
	          "\treturn static_cast<unsigned char>(value * 255.0f + 0.5f);\n"
	          "}\n\n"
	          "static __device__ void fs_main(FsParams params, unsigned int invocationIndex, const FragmentInvocation &invocation, unsigned char &outR, unsigned char &outG, unsigned char &outB, unsigned char &outA)\n"
	          "{\n"
	          "\t(void)invocationIndex;\n";
	if(config.shaderKind == FragmentBootstrapShaderKind::FragCoordQuadrants)
	{
		source << "\tbool left = invocation.x * 2u < params.width;\n"
		          "\tbool top = invocation.y * 2u < params.height;\n"
		          "\toutR = left && top ? 255u : (!left && !top ? 255u : 0u);\n"
		          "\toutG = !left && top ? 255u : (!left && !top ? 255u : 0u);\n"
		          "\toutB = left && !top ? 255u : 0u;\n"
		          "\toutA = 255u;\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor)
	{
		source << "\tfloat colorR = params.vertexColor0R * invocation.barycentric0 + params.vertexColor1R * invocation.barycentric1 + params.vertexColor2R * invocation.barycentric2;\n"
		          "\tfloat colorG = params.vertexColor0G * invocation.barycentric0 + params.vertexColor1G * invocation.barycentric1 + params.vertexColor2G * invocation.barycentric2;\n"
		          "\tfloat colorB = params.vertexColor0B * invocation.barycentric0 + params.vertexColor1B * invocation.barycentric1 + params.vertexColor2B * invocation.barycentric2;\n"
		          "\tfloat colorA = params.vertexColor0A * invocation.barycentric0 + params.vertexColor1A * invocation.barycentric1 + params.vertexColor2A * invocation.barycentric2;\n"
		          "\toutR = packColor(colorR);\n"
		          "\toutG = packColor(colorG);\n"
		          "\toutB = packColor(colorB);\n"
		          "\toutA = packColor(colorA);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::FrontFacingBinaryColors)
	{
		source << "\tbool frontFacing = invocation.frontFacing != 0u;\n"
		          "\tfloat colorR = frontFacing ? params.colorR : params.backColorR;\n"
		          "\tfloat colorG = frontFacing ? params.colorG : params.backColorG;\n"
		          "\tfloat colorB = frontFacing ? params.colorB : params.backColorB;\n"
		          "\tfloat colorA = frontFacing ? params.colorA : params.backColorA;\n"
		          "\toutR = packColor(colorR);\n"
		          "\toutG = packColor(colorG);\n"
		          "\toutB = packColor(colorB);\n"
		          "\toutA = packColor(colorA);\n";
	}
	else
	{
		source << "\t(void)params;\n"
		       << "\toutR = packColor(" << literalFloat(config.colorR) << ");\n"
		       << "\toutG = packColor(" << literalFloat(config.colorG) << ");\n"
		       << "\toutB = packColor(" << literalFloat(config.colorB) << ");\n"
		       << "\toutA = packColor(" << literalFloat(config.colorA) << ");\n";
	}
	source << "}\n\n"
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
	params.colorR = config.colorR;
	params.colorG = config.colorG;
	params.colorB = config.colorB;
	params.colorA = config.colorA;
	params.backColorR = config.backColorR;
	params.backColorG = config.backColorG;
	params.backColorB = config.backColorB;
	params.backColorA = config.backColorA;
	params.vertexColor0R = config.vertexColor0R;
	params.vertexColor0G = config.vertexColor0G;
	params.vertexColor0B = config.vertexColor0B;
	params.vertexColor0A = config.vertexColor0A;
	params.vertexColor1R = config.vertexColor1R;
	params.vertexColor1G = config.vertexColor1G;
	params.vertexColor1B = config.vertexColor1B;
	params.vertexColor1A = config.vertexColor1A;
	params.vertexColor2R = config.vertexColor2R;
	params.vertexColor2G = config.vertexColor2G;
	params.vertexColor2B = config.vertexColor2B;
	params.vertexColor2A = config.vertexColor2A;
	std::vector<void *> arguments = { &params };

	LaunchRecord record = {};
	record.groupCountX = (static_cast<uint32_t>(invocations.size()) + kFragmentBlockWidth - 1) / kFragmentBlockWidth;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = std::min(static_cast<uint32_t>(invocations.size()), kFragmentBlockWidth);
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

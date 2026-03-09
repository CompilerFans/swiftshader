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
	float *depthBuffer = nullptr;
	float colorR = 1.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 1.0f;
	float backColorR = 0.0f;
	float backColorG = 0.0f;
	float backColorB = 1.0f;
	float backColorA = 1.0f;
	float nearDepth = 0.2f;
	float farDepth = 0.8f;
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
	float vertexTexCoord0U = 0.0f;
	float vertexTexCoord0V = 0.0f;
	float vertexTexCoord1U = 1.0f;
	float vertexTexCoord1V = 0.0f;
	float vertexTexCoord2U = 0.0f;
	float vertexTexCoord2V = 1.0f;
	const uint8_t *textureData = nullptr;
	uint32_t textureWidth = 0u;
	uint32_t textureHeight = 0u;
	uint32_t textureRowPitchTexels = 0u;
	uint32_t textureFilterLinear = 0u;
	uint32_t textureAddressModeU = 0u;
	uint32_t textureAddressModeV = 0u;
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
	          "\tfloat pointCoordX;\n"
	          "\tfloat pointCoordY;\n"
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
	          "\tfloat *depthBuffer;\n"
	          "\tfloat colorR;\n"
	          "\tfloat colorG;\n"
	          "\tfloat colorB;\n"
	          "\tfloat colorA;\n"
	          "\tfloat backColorR;\n"
	          "\tfloat backColorG;\n"
	          "\tfloat backColorB;\n"
	          "\tfloat backColorA;\n"
	          "\tfloat nearDepth;\n"
	          "\tfloat farDepth;\n"
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
	          "\tfloat vertexTexCoord0U;\n"
	          "\tfloat vertexTexCoord0V;\n"
	          "\tfloat vertexTexCoord1U;\n"
	          "\tfloat vertexTexCoord1V;\n"
	          "\tfloat vertexTexCoord2U;\n"
	          "\tfloat vertexTexCoord2V;\n"
	          "\tconst unsigned char *textureData;\n"
	          "\tunsigned int textureWidth;\n"
	          "\tunsigned int textureHeight;\n"
	          "\tunsigned int textureRowPitchTexels;\n"
	          "\tunsigned int textureFilterLinear;\n"
	          "\tunsigned int textureAddressModeU;\n"
	          "\tunsigned int textureAddressModeV;\n"
	          "};\n\n"

	          "static __device__ unsigned char packColor(float value)\n"
	          "{\n"
	          "\tvalue = value < 0.0f ? 0.0f : value;\n"
	          "\tvalue = value > 1.0f ? 1.0f : value;\n"
	          "\treturn static_cast<unsigned char>(value * 255.0f + 0.5f);\n"
	          "}\n\n"
	          "static __device__ float applyAddressMode(float coord, unsigned int addressMode)\n"
          "{\n"
          "	if(addressMode != 0u)\n"
          "	{\n"
          "		coord = coord - floorf(coord);\n"
          "		if(coord < 0.0f) coord += 1.0f;\n"
          "		return coord;\n"
          "	}\n"
          "	coord = coord < 0.0f ? 0.0f : coord;\n"
          "	coord = coord > 1.0f ? 1.0f : coord;\n"
          "	return coord;\n"
          "}\n\n"
          "static __device__ void fetchTexel(const FsParams &params, int x, int y, float &r, float &g, float &b, float &a)\n"
          "{\n"
          "	unsigned int offset = (static_cast<unsigned int>(y) * params.textureRowPitchTexels + static_cast<unsigned int>(x)) * 4u;\n"
          "	r = params.textureData[offset + 0] / 255.0f;\n"
          "	g = params.textureData[offset + 1] / 255.0f;\n"
          "	b = params.textureData[offset + 2] / 255.0f;\n"
          "	a = params.textureData[offset + 3] / 255.0f;\n"
          "}\n\n"
          "static __device__ void sampleTexture(const FsParams &params, float u, float v, float &r, float &g, float &b, float &a)\n"
          "{\n"
          "	u = applyAddressMode(u, params.textureAddressModeU);\n"
          "	v = applyAddressMode(v, params.textureAddressModeV);\n"
          "	float fx = u * static_cast<float>(params.textureWidth - 1u);\n"
          "	float fy = v * static_cast<float>(params.textureHeight - 1u);\n"
          "	if(params.textureFilterLinear == 0u)\n"
          "	{\n"
          "		int x = static_cast<int>(fx + 0.5f);\n"
          "		int y = static_cast<int>(fy + 0.5f);\n"
          "		fetchTexel(params, x, y, r, g, b, a);\n"
          "		return;\n"
          "	}\n"
          "	int x0 = static_cast<int>(floorf(fx));\n"
          "	int y0 = static_cast<int>(floorf(fy));\n"
          "	int x1 = x0 + 1;\n"
          "	int y1 = y0 + 1;\n"
          "	if(x1 >= static_cast<int>(params.textureWidth)) x1 = params.textureAddressModeU != 0u ? 0 : static_cast<int>(params.textureWidth - 1u);\n"
          "	if(y1 >= static_cast<int>(params.textureHeight)) y1 = params.textureAddressModeV != 0u ? 0 : static_cast<int>(params.textureHeight - 1u);\n"
          "	float tx = fx - floorf(fx);\n"
          "	float ty = fy - floorf(fy);\n"
          "	float r00,g00,b00,a00,r10,g10,b10,a10,r01,g01,b01,a01,r11,g11,b11,a11;\n"
          "	fetchTexel(params, x0, y0, r00,g00,b00,a00);\n"
          "	fetchTexel(params, x1, y0, r10,g10,b10,a10);\n"
          "	fetchTexel(params, x0, y1, r01,g01,b01,a01);\n"
          "	fetchTexel(params, x1, y1, r11,g11,b11,a11);\n"
          "	float r0 = r00 + (r10 - r00) * tx;\n"
          "	float g0 = g00 + (g10 - g00) * tx;\n"
          "	float b0 = b00 + (b10 - b00) * tx;\n"
          "	float a0 = a00 + (a10 - a00) * tx;\n"
          "	float r1 = r01 + (r11 - r01) * tx;\n"
          "	float g1 = g01 + (g11 - g01) * tx;\n"
          "	float b1 = b01 + (b11 - b01) * tx;\n"
          "	float a1 = a01 + (a11 - a01) * tx;\n"
          "	r = r0 + (r1 - r0) * ty;\n"
          "	g = g0 + (g1 - g0) * ty;\n"
          "	b = b0 + (b1 - b0) * ty;\n"
          "	a = a0 + (a1 - a0) * ty;\n"
          "}\n\n"
          "static __device__ void fs_main(FsParams params, unsigned int invocationIndex, const FragmentInvocation &invocation, unsigned char &outR, unsigned char &outG, unsigned char &outB, unsigned char &outA, float &outDepth)\n"
	          "{\n"
	          "\t(void)invocationIndex;\n";
	if(config.shaderKind == FragmentBootstrapShaderKind::FragCoordQuadrants)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\tbool left = invocation.x * 2u < params.width;\n"
		          "\tbool top = invocation.y * 2u < params.height;\n"
		          "\toutR = left && top ? 255u : (!left && !top ? 255u : 0u);\n"
		          "\toutG = !left && top ? 255u : (!left && !top ? 255u : 0u);\n"
		          "\toutB = left && !top ? 255u : 0u;\n"
		          "\toutA = 255u;\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\tfloat colorR = params.vertexColor0R * invocation.barycentric0 + params.vertexColor1R * invocation.barycentric1 + params.vertexColor2R * invocation.barycentric2;\n"
		          "\tfloat colorG = params.vertexColor0G * invocation.barycentric0 + params.vertexColor1G * invocation.barycentric1 + params.vertexColor2G * invocation.barycentric2;\n"
		          "\tfloat colorB = params.vertexColor0B * invocation.barycentric0 + params.vertexColor1B * invocation.barycentric1 + params.vertexColor2B * invocation.barycentric2;\n"
		          "\tfloat colorA = params.vertexColor0A * invocation.barycentric0 + params.vertexColor1A * invocation.barycentric1 + params.vertexColor2A * invocation.barycentric2;\n"
		          "\toutR = packColor(colorR);\n"
		          "\toutG = packColor(colorG);\n"
		          "\toutB = packColor(colorB);\n"
		          "\toutA = packColor(colorA);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::Texture2DColor)
	{
		source << "	outDepth = 1.0f;\n"
		          "	float u = params.vertexTexCoord0U * invocation.barycentric0 + params.vertexTexCoord1U * invocation.barycentric1 + params.vertexTexCoord2U * invocation.barycentric2;\n"
		          "	float v = params.vertexTexCoord0V * invocation.barycentric0 + params.vertexTexCoord1V * invocation.barycentric1 + params.vertexTexCoord2V * invocation.barycentric2;\n"
		          "	float colorR;\n"
		          "	float colorG;\n"
		          "	float colorB;\n"
		          "	float colorA;\n"
		          "	sampleTexture(params, u, v, colorR, colorG, colorB, colorA);\n"
		          "	outR = packColor(colorR);\n"
		          "	outG = packColor(colorG);\n"
		          "	outB = packColor(colorB);\n"
		          "	outA = packColor(colorA);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::PointCoordGradient)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\toutR = packColor(invocation.pointCoordX);\n"
		          "\toutG = packColor(invocation.pointCoordY);\n"
		          "\toutB = 0u;\n"
		          "\toutA = 255u;\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\toutR = packColor(params.vertexColor0R);\n"
		          "\toutG = packColor(params.vertexColor0G);\n"
		          "\toutB = packColor(params.vertexColor0B);\n"
		          "\toutA = packColor(params.vertexColor0A);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth)
	{
		source << "\tfloat colorR = params.vertexColor0R * invocation.barycentric0 + params.vertexColor1R * invocation.barycentric1 + params.vertexColor2R * invocation.barycentric2;\n"
		          "\tfloat colorG = params.vertexColor0G * invocation.barycentric0 + params.vertexColor1G * invocation.barycentric1 + params.vertexColor2G * invocation.barycentric2;\n"
		          "\tfloat colorB = params.vertexColor0B * invocation.barycentric0 + params.vertexColor1B * invocation.barycentric1 + params.vertexColor2B * invocation.barycentric2;\n"
		          "\tfloat colorA = params.vertexColor0A * invocation.barycentric0 + params.vertexColor1A * invocation.barycentric1 + params.vertexColor2A * invocation.barycentric2;\n"
		          "\toutDepth = colorB > colorR ? params.nearDepth : params.farDepth;\n"
		          "\toutR = packColor(colorR);\n"
		          "\toutG = packColor(colorG);\n"
		          "\toutB = packColor(colorB);\n"
		          "\toutA = packColor(colorA);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::FrontFacingBinaryColors)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\tbool frontFacing = invocation.frontFacing != 0u;\n"
		          "\tfloat colorR = frontFacing ? params.colorR : params.backColorR;\n"
		          "\tfloat colorG = frontFacing ? params.colorG : params.backColorG;\n"
		          "\tfloat colorB = frontFacing ? params.colorB : params.backColorB;\n"
		          "\tfloat colorA = frontFacing ? params.colorA : params.backColorA;\n"
		          "\toutR = packColor(colorR);\n"
		          "\toutG = packColor(colorG);\n"
		          "\toutB = packColor(colorB);\n"
		          "\toutA = packColor(colorA);\n";
	}
	else if(config.shaderKind == FragmentBootstrapShaderKind::FragCoordDiscardLeftConstantColor)
	{
		source << "\toutDepth = 1.0f;\n"
		          "\tif(invocation.x * 2u < params.width)\n"
		          "\t{\n"
		          "\t\toutR = 0u;\n"
		          "\t\toutG = 0u;\n"
		          "\t\toutB = 0u;\n"
		          "\t\toutA = 0u;\n"
		          "\t\treturn;\n"
		          "\t}\n"
		          "\toutR = packColor(params.colorR);\n"
		          "\toutG = packColor(params.colorG);\n"
		          "\toutB = packColor(params.colorB);\n"
		          "\toutA = packColor(params.colorA);\n";
	}
	else
	{
		source << "\toutDepth = 1.0f;\n"
		       << "\t(void)params;\n"
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
	          "\tfloat outDepth = 1.0f;\n"
	          "\tfs_main(params, invocationIndex, invocation, outR, outG, outB, outA, outDepth);\n"
	          "\tunsigned int offset = (invocation.y * params.width + invocation.x) * 4u;\n"
	          "\tparams.colorBuffer[offset + 0] = outR;\n"
	          "\tparams.colorBuffer[offset + 1] = outG;\n"
	          "\tparams.colorBuffer[offset + 2] = outB;\n"
	          "\tparams.colorBuffer[offset + 3] = outA;\n"
	          "\tif(params.depthBuffer != nullptr)\n"
	          "\t{\n"
	          "\t\tparams.depthBuffer[invocation.y * params.width + invocation.x] = outDepth;\n"
	          "\t}\n"
	          "}\n";
	return source.str();
}

bool runFragmentBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, const std::vector<FragmentBootstrapInvocation> &invocations, const FragmentBootstrapConfig &config, std::vector<uint8_t> *colorBuffer, std::vector<float> *depthBuffer)
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
	size_t depthBytes = static_cast<size_t>(width) * height * sizeof(float);

	auto invocationMemory = runtime.allocateMemory(invocationBytes);
	auto colorMemory = runtime.allocateMemory(colorBytes);
	auto depthMemory = depthBuffer ? runtime.allocateMemory(depthBytes) : DeviceMemoryHandle{};
	auto textureMemory = (!config.textureData.empty()) ? runtime.allocateMemory(config.textureData.size()) : DeviceMemoryHandle{};
	if(!invocationMemory.valid() || !colorMemory.valid() || (depthBuffer && !depthMemory.valid()) || (!config.textureData.empty() && !textureMemory.valid()))
	{
		if(depthMemory.valid())
		{
			runtime.freeMemory(depthMemory);
		}
		if(colorMemory.valid())
		{
			runtime.freeMemory(colorMemory);
		}
		if(textureMemory.valid())
		{
			runtime.freeMemory(textureMemory);
		}
		if(invocationMemory.valid())
		{
			runtime.freeMemory(invocationMemory);
		}
		return false;
	}

	std::vector<uint8_t> zeroColor(colorBytes, 0);
	std::vector<float> clearDepth;
	if(depthBuffer)
	{
		clearDepth.assign(static_cast<size_t>(width) * height, 1.0f);
	}
	runtime.copyHostToMemory(invocationMemory, invocations.data(), invocationBytes);
	runtime.copyHostToMemory(colorMemory, zeroColor.data(), zeroColor.size());
	if(!config.textureData.empty())
	{
		runtime.copyHostToMemory(textureMemory, config.textureData.data(), config.textureData.size());
	}
	if(depthBuffer)
	{
		runtime.copyHostToMemory(depthMemory, clearDepth.data(), depthBytes);
	}

	BootstrapFsParams params = {};
	params.invocations = reinterpret_cast<const FragmentBootstrapInvocation *>(static_cast<uintptr_t>(runtime.memoryAddress(invocationMemory)));
	params.colorBuffer = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(runtime.memoryAddress(colorMemory)));
	params.invocationCount = static_cast<uint32_t>(invocations.size());
	params.width = width;
	params.height = height;
	params.depthBuffer = depthBuffer ? reinterpret_cast<float *>(static_cast<uintptr_t>(runtime.memoryAddress(depthMemory))) : nullptr;
	params.colorR = config.colorR;
	params.colorG = config.colorG;
	params.colorB = config.colorB;
	params.colorA = config.colorA;
	params.backColorR = config.backColorR;
	params.backColorG = config.backColorG;
	params.backColorB = config.backColorB;
	params.backColorA = config.backColorA;
	params.nearDepth = config.nearDepth;
	params.farDepth = config.farDepth;
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
	params.vertexTexCoord0U = config.vertexTexCoord0U;
	params.vertexTexCoord0V = config.vertexTexCoord0V;
	params.vertexTexCoord1U = config.vertexTexCoord1U;
	params.vertexTexCoord1V = config.vertexTexCoord1V;
	params.vertexTexCoord2U = config.vertexTexCoord2U;
	params.vertexTexCoord2V = config.vertexTexCoord2V;
	params.textureData = config.textureData.empty() ? nullptr : reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(runtime.memoryAddress(textureMemory)));
	params.textureWidth = config.textureWidth;
	params.textureHeight = config.textureHeight;
	params.textureRowPitchTexels = config.textureRowPitchTexels;
	params.textureFilterLinear = config.textureFilterLinear;
	params.textureAddressModeU = config.textureAddressModeU;
	params.textureAddressModeV = config.textureAddressModeV;
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
	if(depthBuffer)
	{
		depthBuffer->resize(static_cast<size_t>(width) * height);
		runtime.copyMemoryToHost(depthBuffer->data(), depthMemory, depthBytes);
		runtime.freeMemory(depthMemory);
	}

	runtime.freeMemory(colorMemory);
	if(textureMemory.valid())
	{
		runtime.freeMemory(textureMemory);
	}
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

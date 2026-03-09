#include "RasterBootstrap.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <sstream>
#include <vector>

namespace backend {
namespace {

constexpr uint32_t kRasterBlockWidth = 8u;
constexpr uint32_t kRasterBlockHeight = 8u;

struct RasterParams
{
	const RasterBootstrapVertex *vertices = nullptr;
	FragmentBootstrapInvocation *invocations = nullptr;
	uint32_t invocationCount = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t frontFacing = 1;
};

float edgeFunction(const RasterBootstrapVertex &a, const RasterBootstrapVertex &b, float px, float py)
{
	return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

bool pointInsideTriangle(const std::array<RasterBootstrapVertex, 3> &triangle, float px, float py)
{
	float e0 = edgeFunction(triangle[0], triangle[1], px, py);
	float e1 = edgeFunction(triangle[1], triangle[2], px, py);
	float e2 = edgeFunction(triangle[2], triangle[0], px, py);

	bool allNonNegative = (e0 >= 0.0f) && (e1 >= 0.0f) && (e2 >= 0.0f);
	bool allNonPositive = (e0 <= 0.0f) && (e1 <= 0.0f) && (e2 <= 0.0f);
	return allNonNegative || allNonPositive;
}

bool computeFrontFacing(const std::array<RasterBootstrapVertex, 3> &triangle, bool frontFaceCounterClockwise)
{
	float area = edgeFunction(triangle[0], triangle[1], triangle[2].x, triangle[2].y);
	return frontFaceCounterClockwise ? (area >= 0.0f) : (area <= 0.0f);
}

void interpolateBarycentrics(const std::array<RasterBootstrapVertex, 3> &triangle, float px, float py, float *barycentrics)
{
	float denominator0 = edgeFunction(triangle[1], triangle[2], triangle[0].x, triangle[0].y);
	float denominator1 = edgeFunction(triangle[2], triangle[0], triangle[1].x, triangle[1].y);
	float denominator2 = edgeFunction(triangle[0], triangle[1], triangle[2].x, triangle[2].y);

	barycentrics[0] = edgeFunction(triangle[1], triangle[2], px, py) / denominator0;
	barycentrics[1] = edgeFunction(triangle[2], triangle[0], px, py) / denominator1;
	barycentrics[2] = edgeFunction(triangle[0], triangle[1], px, py) / denominator2;
}

}  // namespace

std::string rasterBootstrapCudaSource()
{
	std::ostringstream source;
	source << "struct RasterVertex\n"
	          "{\n"
	          "\tfloat x;\n"
	          "\tfloat y;\n"
	          "\tfloat z;\n"
	          "\tfloat w;\n"
	          "\tfloat colorR;\n"
	          "\tfloat colorG;\n"
	          "\tfloat colorB;\n"
	          "\tfloat colorA;\n"
	          "};\n\n"
	          "struct RasterInvocation\n"
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
	          "struct RasterParams\n"
	          "{\n"
	          "\tconst RasterVertex *vertices;\n"
	          "\tRasterInvocation *invocations;\n"
	          "\tunsigned int invocationCount;\n"
	          "\tunsigned int width;\n"
	          "\tunsigned int height;\n"
	          "\tunsigned int frontFacing;\n"
	          "};\n\n"
	          "static __device__ float edgeFunction(const RasterVertex &a, const RasterVertex &b, float px, float py)\n"
	          "{\n"
	          "\treturn (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);\n"
	          "}\n\n"
	          "static __device__ bool pointInsideTriangle(const RasterVertex &v0, const RasterVertex &v1, const RasterVertex &v2, float px, float py)\n"
	          "{\n"
	          "\tfloat e0 = edgeFunction(v0, v1, px, py);\n"
	          "\tfloat e1 = edgeFunction(v1, v2, px, py);\n"
	          "\tfloat e2 = edgeFunction(v2, v0, px, py);\n"
	          "\tbool allNonNegative = (e0 >= 0.0f) && (e1 >= 0.0f) && (e2 >= 0.0f);\n"
	          "\tbool allNonPositive = (e0 <= 0.0f) && (e1 <= 0.0f) && (e2 <= 0.0f);\n"
	          "\treturn allNonNegative || allNonPositive;\n"
	          "}\n\n"
	          "static __device__ unsigned int computeFrontFacing(const RasterVertex &v0, const RasterVertex &v1, const RasterVertex &v2, unsigned int frontFaceCounterClockwise)\n"
	          "{\n"
	          "\tfloat area = edgeFunction(v0, v1, v2.x, v2.y);\n"
	          "\treturn frontFaceCounterClockwise != 0u ? (area >= 0.0f ? 1u : 0u) : (area <= 0.0f ? 1u : 0u);\n"
	          "}\n\n"
	          "static __device__ void interpolateBarycentrics(const RasterVertex &v0, const RasterVertex &v1, const RasterVertex &v2, float px, float py, float &barycentric0, float &barycentric1, float &barycentric2)\n"
	          "{\n"
	          "\tfloat denominator0 = edgeFunction(v1, v2, v0.x, v0.y);\n"
	          "\tfloat denominator1 = edgeFunction(v2, v0, v1.x, v1.y);\n"
	          "\tfloat denominator2 = edgeFunction(v0, v1, v2.x, v2.y);\n"
	          "\tbarycentric0 = edgeFunction(v1, v2, px, py) / denominator0;\n"
	          "\tbarycentric1 = edgeFunction(v2, v0, px, py) / denominator1;\n"
	          "\tbarycentric2 = edgeFunction(v0, v1, px, py) / denominator2;\n"
	          "}\n\n"
	          "extern \"C\" __global__ void raster_entry(RasterParams params)\n"
	          "{\n"
	          "\tunsigned int x = blockIdx.x * blockDim.x + threadIdx.x;\n"
	          "\tunsigned int y = blockIdx.y * blockDim.y + threadIdx.y;\n"
	          "\tif(x >= params.width || y >= params.height)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tunsigned int index = y * params.width + x;\n"
	          "\tif(index >= params.invocationCount)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tRasterInvocation invocation = {};\n"
	          "\tinvocation.x = x;\n"
	          "\tinvocation.y = y;\n"
	          "\tinvocation.exportMask = 0u;\n"
	          "\tinvocation.helperInvocation = 0u;\n"
	          "\tinvocation.frontFacing = params.frontFacing;\n"
	          "\tinvocation.barycentric0 = 0.0f;\n"
	          "\tinvocation.barycentric1 = 0.0f;\n"
	          "\tinvocation.barycentric2 = 0.0f;\n"
	          "\tfloat px = static_cast<float>(x) + 0.5f;\n"
	          "\tfloat py = static_cast<float>(y) + 0.5f;\n"
	          "\tif(pointInsideTriangle(params.vertices[0], params.vertices[1], params.vertices[2], px, py))\n"
	          "\t{\n"
	          "\t\tinvocation.exportMask = 1u;\n"
	          "\t\tinterpolateBarycentrics(params.vertices[0], params.vertices[1], params.vertices[2], px, py, invocation.barycentric0, invocation.barycentric1, invocation.barycentric2);\n"
	          "\t}\n"
	          "\tparams.invocations[index] = invocation;\n"
	          "}\n";
	return source.str();
}

RasterBootstrapOutput rasterBootstrapCpuReference(const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &config)
{
	RasterBootstrapOutput output = {};
	if(config.width == 0 || config.height == 0)
	{
		return output;
	}

	float minX = std::min({ triangle[0].x, triangle[1].x, triangle[2].x });
	float minY = std::min({ triangle[0].y, triangle[1].y, triangle[2].y });
	float maxX = std::max({ triangle[0].x, triangle[1].x, triangle[2].x });
	float maxY = std::max({ triangle[0].y, triangle[1].y, triangle[2].y });

	uint32_t bboxMinX = static_cast<uint32_t>(std::max(0.0f, std::floor(minX)));
	uint32_t bboxMinY = static_cast<uint32_t>(std::max(0.0f, std::floor(minY)));
	uint32_t bboxMaxX = static_cast<uint32_t>(std::min(static_cast<float>(config.width - 1), std::ceil(maxX) - 1.0f));
	uint32_t bboxMaxY = static_cast<uint32_t>(std::min(static_cast<float>(config.height - 1), std::ceil(maxY) - 1.0f));

	output.valid = true;
	output.bboxMinX = bboxMinX;
	bool frontFacing = computeFrontFacing(triangle, config.frontFaceCounterClockwise);
	output.bboxMinY = bboxMinY;
	output.bboxMaxX = bboxMaxX;
	output.bboxMaxY = bboxMaxY;

	for(uint32_t y = bboxMinY; y <= bboxMaxY; y++)
	{
		for(uint32_t x = bboxMinX; x <= bboxMaxX; x++)
		{
			float px = static_cast<float>(x) + 0.5f;
			float py = static_cast<float>(y) + 0.5f;
			if(!pointInsideTriangle(triangle, px, py))
			{
				continue;
			}

			FragmentBootstrapInvocation invocation = {};
			invocation.x = x;
			invocation.y = y;
			invocation.exportMask = 1u;
			invocation.helperInvocation = 0u;
			invocation.frontFacing = frontFacing ? 1u : 0u;
			float barycentrics[3] = {};
			interpolateBarycentrics(triangle, px, py, barycentrics);
			invocation.barycentric0 = barycentrics[0];
			invocation.barycentric1 = barycentrics[1];
			invocation.barycentric2 = barycentrics[2];
			output.invocations.push_back(invocation);
		}
	}

	return output;
}

bool runRasterBootstrap(RuntimeAPI &runtime, const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &config, RasterBootstrapOutput *output)
{
	auto module = runtime.createModule(rasterBootstrapCudaSource(), "raster_entry");
	if(!module.valid())
	{
		return false;
	}

	RasterParams params = {};
	params.width = config.width;
	params.height = config.height;
	params.frontFacing = computeFrontFacing(triangle, config.frontFaceCounterClockwise) ? 1u : 0u;
	params.invocationCount = config.width * config.height;

	if(!runtime.isHardwareBacked())
	{
		std::vector<void *> arguments = { &params };

		LaunchRecord record = {};
		record.groupCountX = 1;
		record.groupCountY = 1;
		record.groupCountZ = 1;
		record.blockCountX = 1;
		record.blockCountY = 1;
		record.blockCountZ = 1;
		record.argumentCount = arguments.size();
		runtime.launch(module, record, arguments);
		runtime.synchronize();

		if(output)
		{
			*output = rasterBootstrapCpuReference(triangle, config);
		}
		return true;
	}

	size_t vertexBytes = sizeof(RasterBootstrapVertex) * triangle.size();
	size_t invocationBytes = sizeof(FragmentBootstrapInvocation) * static_cast<size_t>(config.width) * config.height;
	auto vertexMemory = runtime.allocateMemory(vertexBytes);
	auto invocationMemory = runtime.allocateMemory(invocationBytes);
	if(!vertexMemory.valid() || !invocationMemory.valid())
	{
		if(invocationMemory.valid())
		{
			runtime.freeMemory(invocationMemory);
		}
		if(vertexMemory.valid())
		{
			runtime.freeMemory(vertexMemory);
		}
		return false;
	}

	std::vector<FragmentBootstrapInvocation> denseInvocations(static_cast<size_t>(config.width) * config.height);
	runtime.copyHostToMemory(vertexMemory, triangle.data(), vertexBytes);
	runtime.copyHostToMemory(invocationMemory, denseInvocations.data(), invocationBytes);

	params.vertices = reinterpret_cast<const RasterBootstrapVertex *>(static_cast<uintptr_t>(runtime.memoryAddress(vertexMemory)));
	params.invocations = reinterpret_cast<FragmentBootstrapInvocation *>(static_cast<uintptr_t>(runtime.memoryAddress(invocationMemory)));
	std::vector<void *> arguments = { &params };

	LaunchRecord record = {};
	record.groupCountX = (config.width + kRasterBlockWidth - 1) / kRasterBlockWidth;
	record.groupCountY = (config.height + kRasterBlockHeight - 1) / kRasterBlockHeight;
	record.groupCountZ = 1;
	record.blockCountX = std::min(config.width, kRasterBlockWidth);
	record.blockCountY = std::min(config.height, kRasterBlockHeight);
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();
	runtime.launch(module, record, arguments);
	runtime.synchronize();

	if(output)
	{
		*output = rasterBootstrapCpuReference(triangle, config);
		output->invocations.clear();
		runtime.copyMemoryToHost(denseInvocations.data(), invocationMemory, invocationBytes);
		for(const auto &invocation : denseInvocations)
		{
			if(invocation.exportMask != 0u)
			{
				output->invocations.push_back(invocation);
			}
		}
	}

	runtime.freeMemory(invocationMemory);
	runtime.freeMemory(vertexMemory);
	return true;
}

bool runRasterFragmentBootstrap(RuntimeAPI &runtime, const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &rasterConfig, const FragmentBootstrapConfig &fragmentConfig, std::vector<uint8_t> *colorBuffer)
{
	RasterBootstrapOutput rasterOutput = {};
	if(!runRasterBootstrap(runtime, triangle, rasterConfig, &rasterOutput))
	{
		return false;
	}

	if(rasterOutput.invocations.empty())
	{
		if(colorBuffer)
		{
			colorBuffer->assign(static_cast<size_t>(rasterConfig.width) * rasterConfig.height * 4u, 0u);
		}
		return true;
	}

	return runFragmentBootstrap(runtime, rasterConfig.width, rasterConfig.height, rasterOutput.invocations, fragmentConfig, colorBuffer);
}

void launchRasterBootstrap(RuntimeAPI &runtime)
{
	std::array<RasterBootstrapVertex, 3> triangle = {{
		RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
		RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
	}};

	RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;
	runRasterBootstrap(runtime, triangle, config, nullptr);
}

}  // namespace backend

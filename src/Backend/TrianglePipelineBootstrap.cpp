#include "TrianglePipelineBootstrap.hpp"

#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RasterBootstrap.hpp"

#include <array>
#include <vector>

namespace backend {
namespace {

std::vector<GraphicsBootstrapVertexInput> bootstrapTriangleInputs()
{
	return {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};
}

std::array<RasterBootstrapVertex, 3> toRasterVertices(const std::vector<GraphicsBootstrapVertexOutput> &outputs, uint32_t width, uint32_t height)
{
	std::array<RasterBootstrapVertex, 3> triangle = {};
	for(size_t i = 0; i < 3 && i < outputs.size(); i++)
	{
		float ndcX = outputs[i].x / outputs[i].w;
		float ndcY = outputs[i].y / outputs[i].w;
		triangle[i].x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
		triangle[i].y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height);
		triangle[i].z = outputs[i].z;
		triangle[i].w = outputs[i].w;
	}
	return triangle;
}

}  // namespace

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, std::vector<uint8_t> *colorBuffer)
{
	if(width == 0 || height == 0)
	{
		return false;
	}

	std::vector<GraphicsBootstrapVertexOutput> vsOutputs;
	if(runtime.isHardwareBacked())
	{
		if(!runGraphicsBootstrap(runtime, bootstrapTriangleInputs(), &vsOutputs))
		{
			return false;
		}
	}
	else
	{
		vsOutputs = {
			{ -0.5f, -0.25f, 0.0f, 1.0f },
			{ 0.0f, 0.75f, 0.0f, 1.0f },
			{ 0.5f, -0.25f, 0.0f, 1.0f },
		};
	}

	RasterBootstrapConfig rasterConfig = {};
	rasterConfig.width = width;
	rasterConfig.height = height;

	FragmentBootstrapConfig fragmentConfig = {};
	fragmentConfig.colorR = 0.0f;
	fragmentConfig.colorG = 1.0f;
	fragmentConfig.colorB = 0.0f;
	fragmentConfig.colorA = 1.0f;

	return runRasterFragmentBootstrap(runtime, toRasterVertices(vsOutputs, width, height), rasterConfig, fragmentConfig, colorBuffer);
}

void launchTrianglePipelineBootstrap(RuntimeAPI &runtime)
{
	if(runtime.isHardwareBacked())
	{
		runTrianglePipelineBootstrap(runtime, 64u, 64u, nullptr);
		return;
	}

	launchGraphicsBootstrap(runtime);
	launchRasterBootstrap(runtime);
	launchFragmentBootstrap(runtime);
}

}  // namespace backend

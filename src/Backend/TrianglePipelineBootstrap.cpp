#include "TrianglePipelineBootstrap.hpp"

#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RasterBootstrap.hpp"

#include <array>
#include <vector>

namespace backend {
namespace {
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

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const TrianglePipelineBootstrapConfig &config, std::vector<uint8_t> *colorBuffer)
{
	if(config.width == 0 || config.height == 0)
	{
		return false;
	}

	std::vector<GraphicsBootstrapVertexOutput> vsOutputs;
	if(runtime.isHardwareBacked())
	{
		if(!config.rawVertexData.empty() && config.vertexCount != 0)
		{
			if(!runGraphicsBootstrap(runtime, config.rawVertexData, config.vertexCount, config.binding, GraphicsBootstrapShaderConfig{}, GraphicsBootstrapRuntimeConfig{}, &vsOutputs))
			{
				return false;
			}
		}
		else
		{
			std::vector<GraphicsBootstrapVertexInput> vertexInputs(config.vertices.begin(), config.vertices.end());
			if(!runGraphicsBootstrap(runtime, vertexInputs, &vsOutputs))
			{
				return false;
			}
		}
	}
	else if(!config.rawVertexData.empty() && config.vertexCount != 0)
	{
		if(config.vertexCount != 3)
		{
			return false;
		}
		vsOutputs.resize(config.vertexCount);
		for(uint32_t i = 0; i < config.vertexCount; i++)
		{
			const uint8_t *vertexBase = config.rawVertexData.data() + i * config.binding.vertexStride + config.binding.positionOffset;
			const float *position = reinterpret_cast<const float *>(vertexBase);
			vsOutputs[i] = { position[0], position[1], position[2], 1.0f };
		}
	}
	else
	{
		vsOutputs.resize(config.vertices.size());
		for(size_t i = 0; i < config.vertices.size(); i++)
		{
			vsOutputs[i] = { config.vertices[i].x, config.vertices[i].y, config.vertices[i].z, 1.0f };
		}
	}

	RasterBootstrapConfig rasterConfig = {};
	FragmentBootstrapConfig fragmentConfig = {};
	rasterConfig.width = config.width;
	rasterConfig.height = config.height;

	fragmentConfig.colorR = config.colorR;
	fragmentConfig.colorG = config.colorG;
	fragmentConfig.colorB = config.colorB;
	fragmentConfig.colorA = config.colorA;

	return runRasterFragmentBootstrap(runtime, toRasterVertices(vsOutputs, config.width, config.height), rasterConfig, fragmentConfig, colorBuffer);
}

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, std::vector<uint8_t> *colorBuffer)
{
	TrianglePipelineBootstrapConfig config = {};
	config.width = width;
	config.height = height;
	return runTrianglePipelineBootstrap(runtime, config, colorBuffer);
}

void launchTrianglePipelineBootstrap(RuntimeAPI &runtime)
{
	if(runtime.isHardwareBacked())
	{
		runTrianglePipelineBootstrap(runtime, TrianglePipelineBootstrapConfig{}, nullptr);
		return;
	}

	launchGraphicsBootstrap(runtime);
	launchRasterBootstrap(runtime);
	launchFragmentBootstrap(runtime);
}

}  // namespace backend

#include "TrianglePipelineBootstrap.hpp"

#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RasterBootstrap.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace backend {
namespace {
template<typename IndexType>
bool expandIndexedVertices(const sw::Stream &positionStream, uint32_t vertexCount, const IndexType *indices, int32_t baseVertex, std::vector<uint8_t> *rawVertexData)
{
	if(positionStream.buffer == nullptr || rawVertexData == nullptr)
	{
		return false;
	}

	rawVertexData->resize(static_cast<size_t>(positionStream.vertexStride) * vertexCount);
	auto *destination = rawVertexData->data();
	auto *sourceBase = static_cast<const uint8_t *>(positionStream.buffer);
	for(uint32_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++)
	{
		int64_t sourceIndex = static_cast<int64_t>(indices[vertexIndex]) + baseVertex;
		if(sourceIndex < 0)
		{
			return false;
		}

		std::memcpy(destination + static_cast<size_t>(vertexIndex) * positionStream.vertexStride,
		            sourceBase + static_cast<size_t>(sourceIndex) * positionStream.vertexStride,
		            positionStream.vertexStride);
	}

	return true;
}

uint32_t positionComponentCount(VkFormat format)
{
	switch(format)
	{
	case VK_FORMAT_R32G32_SFLOAT:
		return 2;
	case VK_FORMAT_R32G32B32_SFLOAT:
		return 3;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return 4;
	default:
		return 0;
	}
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
		triangle[i].colorR = outputs[i].colorR;
		triangle[i].colorG = outputs[i].colorG;
		triangle[i].colorB = outputs[i].colorB;
		triangle[i].colorA = outputs[i].colorA;
	}
	return triangle;
}

std::array<RasterBootstrapVertex, 3> toRasterVertices(const GraphicsBootstrapVertexOutput *outputs, uint32_t width, uint32_t height)
{
	std::vector<GraphicsBootstrapVertexOutput> triangleOutputs(outputs, outputs + 3);
	return toRasterVertices(triangleOutputs, width, height);
}

}  // namespace

bool buildTrianglePipelineBootstrapConfig(const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, TrianglePipelineBootstrapConfig *config, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise)
{
	if(config == nullptr || positionStream.buffer == nullptr)
	{
		return false;
	}
	if(topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || primitiveCount == 0)
	{
		return false;
	}
	if(positionStream.inputRate != VK_VERTEX_INPUT_RATE_VERTEX || positionStream.vertexStride == 0)
	{
		return false;
	}

	const uint32_t componentCount = positionComponentCount(positionStream.format);
	if(componentCount == 0)
	{
		return false;
	}

	config->width = renderArea.extent.width;
	config->height = renderArea.extent.height;
	config->vertexCount = primitiveCount * 3;
	if(indexData != nullptr)
	{
		switch(indexType)
		{
		case VK_INDEX_TYPE_UINT16:
			if(!expandIndexedVertices(positionStream, config->vertexCount, static_cast<const uint16_t *>(indexData), baseVertex, &config->rawVertexData))
			{
				return false;
			}
			break;
		case VK_INDEX_TYPE_UINT32:
			if(!expandIndexedVertices(positionStream, config->vertexCount, static_cast<const uint32_t *>(indexData), baseVertex, &config->rawVertexData))
			{
				return false;
			}
			break;
		case VK_INDEX_TYPE_UINT8_EXT:
			if(!expandIndexedVertices(positionStream, config->vertexCount, static_cast<const uint8_t *>(indexData), baseVertex, &config->rawVertexData))
			{
				return false;
			}
			break;
		default:
			return false;
		}
	}
	else
	{
		config->rawVertexData.resize(positionStream.vertexStride * config->vertexCount);
		std::memcpy(config->rawVertexData.data(), positionStream.buffer, config->rawVertexData.size());
	}
	config->frontFaceCounterClockwise = frontFaceCounterClockwise;
	config->binding.vertexStride = positionStream.vertexStride;
	config->binding.positionOffset = 0;
	config->binding.positionComponentCount = componentCount;
	config->binding.colorOffset = 0;
	config->binding.colorComponentCount = 0;
	if(colorStream &&
	   colorStream->buffer != nullptr &&
	   colorStream->binding == positionStream.binding &&
	   colorStream->inputRate == positionStream.inputRate &&
	   colorStream->vertexStride == positionStream.vertexStride)
	{
		const uint32_t colorComponentCount = positionComponentCount(colorStream->format);
		auto positionBase = static_cast<const uint8_t *>(positionStream.buffer);
		auto colorBase = static_cast<const uint8_t *>(colorStream->buffer);
		if(colorComponentCount >= 3 && colorBase >= positionBase)
		{
			config->binding.colorOffset = static_cast<uint32_t>(colorBase - positionBase);
			config->binding.colorComponentCount = colorComponentCount;
		}
	}
	if(fragmentConfig)
	{
		config->fragmentConfig = *fragmentConfig;
		if(fragmentConfig->shaderKind == FragmentBootstrapShaderKind::ConstantColor)
		{
			config->colorR = fragmentConfig->colorR;
			config->colorG = fragmentConfig->colorG;
			config->colorB = fragmentConfig->colorB;
			config->colorA = fragmentConfig->colorA;
		}
	}
	return true;
}

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
			float z = config.binding.positionComponentCount > 2 ? position[2] : 0.0f;
			float colorR = 1.0f;
			float colorG = 1.0f;
			float colorB = 1.0f;
			float colorA = 1.0f;
			if(config.binding.colorComponentCount != 0)
			{
				const float *color = reinterpret_cast<const float *>(config.rawVertexData.data() + i * config.binding.vertexStride + config.binding.colorOffset);
				colorR = color[0];
				colorG = config.binding.colorComponentCount > 1 ? color[1] : colorR;
				colorB = config.binding.colorComponentCount > 2 ? color[2] : colorG;
				colorA = config.binding.colorComponentCount > 3 ? color[3] : 1.0f;
			}
			vsOutputs[i] = { position[0], position[1], z, 1.0f, colorR, colorG, colorB, colorA };
		}
	}
	else
	{
		vsOutputs.resize(config.vertices.size());
		for(size_t i = 0; i < config.vertices.size(); i++)
		{
			vsOutputs[i] = { config.vertices[i].x, config.vertices[i].y, config.vertices[i].z, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		}
	}

	RasterBootstrapConfig rasterConfig = {};
	FragmentBootstrapConfig fragmentConfig = config.fragmentConfig;
	rasterConfig.width = config.width;
	rasterConfig.height = config.height;
	rasterConfig.frontFaceCounterClockwise = config.frontFaceCounterClockwise;

	if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::ConstantColor)
	{
		fragmentConfig.colorR = config.colorR;
		fragmentConfig.colorG = config.colorG;
		fragmentConfig.colorB = config.colorB;
		fragmentConfig.colorA = config.colorA;
	}

	if(vsOutputs.empty() || (vsOutputs.size() % 3) != 0)
	{
		return false;
	}

	const size_t triangleCount = vsOutputs.size() / 3;
	std::vector<uint8_t> accumulatedColorBuffer;
	if(colorBuffer)
	{
		accumulatedColorBuffer.assign(static_cast<size_t>(config.width) * config.height * 4u, 0u);
	}

	for(size_t triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
	{
		const auto triangle = toRasterVertices(vsOutputs.data() + triangleIndex * 3, config.width, config.height);
		if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor)
		{
			fragmentConfig.vertexColor0R = triangle[0].colorR;
			fragmentConfig.vertexColor0G = triangle[0].colorG;
			fragmentConfig.vertexColor0B = triangle[0].colorB;
			fragmentConfig.vertexColor0A = triangle[0].colorA;
			fragmentConfig.vertexColor1R = triangle[1].colorR;
			fragmentConfig.vertexColor1G = triangle[1].colorG;
			fragmentConfig.vertexColor1B = triangle[1].colorB;
			fragmentConfig.vertexColor1A = triangle[1].colorA;
			fragmentConfig.vertexColor2R = triangle[2].colorR;
			fragmentConfig.vertexColor2G = triangle[2].colorG;
			fragmentConfig.vertexColor2B = triangle[2].colorB;
			fragmentConfig.vertexColor2A = triangle[2].colorA;
		}
		std::vector<uint8_t> triangleColorBuffer;
		if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr))
		{
			return false;
		}

		if(colorBuffer)
		{
			for(size_t i = 0; i < triangleColorBuffer.size(); i += 4)
			{
				if(triangleColorBuffer[i + 3] != 0u)
				{
					accumulatedColorBuffer[i + 0] = triangleColorBuffer[i + 0];
					accumulatedColorBuffer[i + 1] = triangleColorBuffer[i + 1];
					accumulatedColorBuffer[i + 2] = triangleColorBuffer[i + 2];
					accumulatedColorBuffer[i + 3] = triangleColorBuffer[i + 3];
				}
			}
		}
	}

	if(colorBuffer)
	{
		*colorBuffer = std::move(accumulatedColorBuffer);
	}

	return true;
}

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, std::vector<uint8_t> *colorBuffer, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise)
{
	TrianglePipelineBootstrapConfig config = {};
	if(!buildTrianglePipelineBootstrapConfig(positionStream, colorStream, topology, primitiveCount, renderArea, &config, fragmentConfig, indexData, indexType, baseVertex, frontFaceCounterClockwise))
	{
		return false;
	}

	return runTrianglePipelineBootstrap(runtime, config, colorBuffer);
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

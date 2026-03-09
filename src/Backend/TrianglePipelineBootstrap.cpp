#include "TrianglePipelineBootstrap.hpp"

#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RasterBootstrap.hpp"

#include <array>
#include <cmath>
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

struct ScreenVertex
{
	float x;
	float y;
};

ScreenVertex projectVertex(const GraphicsBootstrapVertexOutput &output, uint32_t width, uint32_t height)
{
	float ndcX = output.x / output.w;
	float ndcY = output.y / output.w;
	return { (ndcX * 0.5f + 0.5f) * static_cast<float>(width),
	         (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height) };
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
		auto projected = projectVertex(outputs[i], width, height);
		triangle[i].x = projected.x;
		triangle[i].y = projected.y;
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


std::array<RasterBootstrapVertex, 4> toLineQuad(const GraphicsBootstrapVertexOutput &v0, const GraphicsBootstrapVertexOutput &v1, uint32_t width, uint32_t height, float lineWidth)
{
	auto pa = projectVertex(v0, width, height);
	auto pb = projectVertex(v1, width, height);
	RasterBootstrapVertex a = {};
	a.x = pa.x; a.y = pa.y; a.z = v0.z; a.w = v0.w; a.colorR = v0.colorR; a.colorG = v0.colorG; a.colorB = v0.colorB; a.colorA = v0.colorA;
	RasterBootstrapVertex b = {};
	b.x = pb.x; b.y = pb.y; b.z = v1.z; b.w = v1.w; b.colorR = v1.colorR; b.colorG = v1.colorG; b.colorB = v1.colorB; b.colorA = v1.colorA;
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float length = std::sqrt(dx * dx + dy * dy);
	if(length == 0.0f)
	{
		length = 1.0f;
	}
	float nx = -dy / length;
	float ny = dx / length;
	float halfWidth = lineWidth * 0.5f;
	std::array<RasterBootstrapVertex, 4> quad = {};
	auto fill = [&](RasterBootstrapVertex &out, float x, float y, const GraphicsBootstrapVertexOutput &src) {
		out.x = x;
		out.y = y;
		out.z = src.z;
		out.w = src.w;
		out.colorR = src.colorR;
		out.colorG = src.colorG;
		out.colorB = src.colorB;
		out.colorA = src.colorA;
	};
	fill(quad[0], a.x - nx * halfWidth, a.y - ny * halfWidth, v0);
	fill(quad[1], a.x + nx * halfWidth, a.y + ny * halfWidth, v0);
	fill(quad[2], b.x + nx * halfWidth, b.y + ny * halfWidth, v1);
	fill(quad[3], b.x - nx * halfWidth, b.y - ny * halfWidth, v1);
	return quad;
}
std::array<RasterBootstrapVertex, 4> toPointQuad(const GraphicsBootstrapVertexOutput &output, uint32_t width, uint32_t height, float pointSize)
{
	std::array<RasterBootstrapVertex, 4> quad = {};
	float ndcX = output.x / output.w;
	float ndcY = output.y / output.w;
	float centerX = (ndcX * 0.5f + 0.5f) * static_cast<float>(width);
	float centerY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height);
	float halfSize = pointSize * 0.5f;
	auto fill = [&](RasterBootstrapVertex &vertex, float x, float y) {
		vertex.x = x;
		vertex.y = y;
		vertex.z = output.z;
		vertex.w = output.w;
		vertex.colorR = output.colorR;
		vertex.colorG = output.colorG;
		vertex.colorB = output.colorB;
		vertex.colorA = output.colorA;
	};
	fill(quad[0], centerX - halfSize, centerY - halfSize);
	fill(quad[1], centerX + halfSize, centerY - halfSize);
	fill(quad[2], centerX + halfSize, centerY + halfSize);
	fill(quad[3], centerX - halfSize, centerY + halfSize);
	return quad;
}

}  // namespace

bool buildTrianglePipelineBootstrapConfig(const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, TrianglePipelineBootstrapConfig *config, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise, float pointSize)
{
	if(config == nullptr || positionStream.buffer == nullptr)
	{
		return false;
	}
	if((topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST && topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN && topology != VK_PRIMITIVE_TOPOLOGY_LINE_LIST && topology != VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) || primitiveCount == 0)
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
	config->topology = topology;
	config->pointSize = pointSize;
	config->vertexCount = topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? primitiveCount : ((topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST) ? primitiveCount * 2u : ((topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) ? primitiveCount + (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ? 1u : 2u) : primitiveCount * 3u));
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
			vsOutputs[i] = { position[0], position[1], z, 1.0f, config.pointSize, colorR, colorG, colorB, colorA };
		}
	}
	else
	{
		vsOutputs.resize(config.vertices.size());
		for(size_t i = 0; i < config.vertices.size(); i++)
		{
			vsOutputs[i] = { config.vertices[i].x, config.vertices[i].y, config.vertices[i].z, 1.0f, config.pointSize, 1.0f, 1.0f, 1.0f, 1.0f };
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

	if(vsOutputs.empty())
	{
		return false;
	}
	if(config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && (vsOutputs.size() % 3) != 0)
	{
		return false;
	}
	if((config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) && vsOutputs.size() < 3)
	{
		return false;
	}

	const size_t primitiveCount = config.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? vsOutputs.size() : (config.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ? (vsOutputs.size() / 2) : (config.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ? (vsOutputs.size() - 1) : ((config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) ? (vsOutputs.size() - 2) : (vsOutputs.size() / 3))));
	std::vector<uint8_t> accumulatedColorBuffer;
	std::vector<float> accumulatedDepthBuffer;
	const bool useDepthCompose = (fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth);
	if(colorBuffer)
	{
		accumulatedColorBuffer.assign(static_cast<size_t>(config.width) * config.height * 4u, 0u);
	}
	if(useDepthCompose)
	{
		accumulatedDepthBuffer.assign(static_cast<size_t>(config.width) * config.height, 1.0f);
	}

	auto composeColorBuffers = [&](const std::vector<uint8_t> &triangleColorBuffer, const std::vector<float> &triangleDepthBuffer) {
		if(!colorBuffer)
		{
			return;
		}
		for(size_t i = 0; i < triangleColorBuffer.size(); i += 4)
		{
			if(triangleColorBuffer[i + 3] == 0u)
			{
				continue;
			}
			if(useDepthCompose)
			{
				size_t pixelIndex = i / 4;
				if(triangleDepthBuffer[pixelIndex] >= accumulatedDepthBuffer[pixelIndex])
				{
					continue;
				}
				accumulatedDepthBuffer[pixelIndex] = triangleDepthBuffer[pixelIndex];
			}
			accumulatedColorBuffer[i + 0] = triangleColorBuffer[i + 0];
			accumulatedColorBuffer[i + 1] = triangleColorBuffer[i + 1];
			accumulatedColorBuffer[i + 2] = triangleColorBuffer[i + 2];
			accumulatedColorBuffer[i + 3] = triangleColorBuffer[i + 3];
		}
	};

	for(size_t primitiveIndex = 0; primitiveIndex < primitiveCount; primitiveIndex++)
	{
		if(config.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		{
			const float pointSize = vsOutputs[primitiveIndex].pointSize > 0.0f ? vsOutputs[primitiveIndex].pointSize : config.pointSize;
			const auto quad = toPointQuad(vsOutputs[primitiveIndex], config.width, config.height, pointSize);
			float minX = quad[0].x;
			float minY = quad[0].y;
			float maxX = quad[2].x;
			float maxY = quad[2].y;
			uint32_t x0 = static_cast<uint32_t>(std::max(0.0f, std::floor(minX)));
			uint32_t y0 = static_cast<uint32_t>(std::max(0.0f, std::floor(minY)));
			uint32_t x1 = static_cast<uint32_t>(std::min(static_cast<float>(config.width - 1), std::ceil(maxX) - 1.0f));
			uint32_t y1 = static_cast<uint32_t>(std::min(static_cast<float>(config.height - 1), std::ceil(maxY) - 1.0f));
			std::vector<FragmentBootstrapInvocation> invocations;
			invocations.reserve((x1 - x0 + 1) * (y1 - y0 + 1));
			for(uint32_t y = y0; y <= y1; y++)
			{
				for(uint32_t x = x0; x <= x1; x++)
				{
					FragmentBootstrapInvocation invocation = {};
					invocation.x = x;
					invocation.y = y;
					invocation.exportMask = 1u;
					invocation.pointCoordX = (static_cast<float>(x) + 0.5f - minX) / pointSize;
					invocation.pointCoordY = (static_cast<float>(y) + 0.5f - minY) / pointSize;
					invocations.push_back(invocation);
				}
			}
			std::vector<uint8_t> pointColorBuffer;
			if(!runFragmentBootstrap(runtime, config.width, config.height, invocations, fragmentConfig, colorBuffer ? &pointColorBuffer : nullptr, nullptr))
			{
				return false;
			}
			composeColorBuffers(pointColorBuffer, std::vector<float>{});
			continue;
		}

		if(config.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
		{
			const auto quad = toLineQuad(vsOutputs[primitiveIndex], vsOutputs[primitiveIndex + 1], config.width, config.height, config.lineWidth);
			std::array<std::array<RasterBootstrapVertex, 3>, 2> triangles = {{
				{{ quad[0], quad[1], quad[2] }},
				{{ quad[0], quad[2], quad[3] }},
			}};
			for(const auto &triangle : triangles)
			{
				std::vector<uint8_t> triangleColorBuffer;
				std::vector<float> triangleDepthBuffer;
				if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr, useDepthCompose ? &triangleDepthBuffer : nullptr))
				{
					return false;
				}
				composeColorBuffers(triangleColorBuffer, triangleDepthBuffer);
			}
			continue;
		}

		if(config.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
		{
			const auto quad = toLineQuad(vsOutputs[primitiveIndex * 2], vsOutputs[primitiveIndex * 2 + 1], config.width, config.height, config.lineWidth);
			std::array<std::array<RasterBootstrapVertex, 3>, 2> triangles = {{
				{{ quad[0], quad[1], quad[2] }},
				{{ quad[0], quad[2], quad[3] }},
			}};
			for(const auto &triangle : triangles)
			{
				std::vector<uint8_t> triangleColorBuffer;
				std::vector<float> triangleDepthBuffer;
				if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr, useDepthCompose ? &triangleDepthBuffer : nullptr))
				{
					return false;
				}
				composeColorBuffers(triangleColorBuffer, triangleDepthBuffer);
			}
			continue;
		}

		if(config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
		{
			GraphicsBootstrapVertexOutput fanVertices[3] = {
				vsOutputs[0],
				vsOutputs[primitiveIndex + 1],
				vsOutputs[primitiveIndex + 2],
			};
			const auto triangle = toRasterVertices(fanVertices, config.width, config.height);
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor)
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
			std::vector<float> triangleDepthBuffer;
			if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr, useDepthCompose ? &triangleDepthBuffer : nullptr))
			{
				return false;
			}
			composeColorBuffers(triangleColorBuffer, triangleDepthBuffer);
			continue;
		}

		if(config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
		{
			GraphicsBootstrapVertexOutput stripVertices[3] = {
				vsOutputs[primitiveIndex + 0],
				vsOutputs[primitiveIndex + 1 + (primitiveIndex & 1)],
				vsOutputs[primitiveIndex + 2 - (primitiveIndex & 1)],
			};
			const auto triangle = toRasterVertices(stripVertices, config.width, config.height);
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor)
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
			std::vector<float> triangleDepthBuffer;
			if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr, useDepthCompose ? &triangleDepthBuffer : nullptr))
			{
				return false;
			}
			composeColorBuffers(triangleColorBuffer, triangleDepthBuffer);
			continue;
		}

		const auto triangle = toRasterVertices(vsOutputs.data() + primitiveIndex * 3, config.width, config.height);
		if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth || fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor)
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
		std::vector<float> triangleDepthBuffer;
		if(!runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, colorBuffer ? &triangleColorBuffer : nullptr, useDepthCompose ? &triangleDepthBuffer : nullptr))
		{
			return false;
		}
		composeColorBuffers(triangleColorBuffer, triangleDepthBuffer);
	}

	if(colorBuffer)
	{
		*colorBuffer = std::move(accumulatedColorBuffer);
	}

	return true;
}

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, std::vector<uint8_t> *colorBuffer, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise, float pointSize)
{
	TrianglePipelineBootstrapConfig config = {};
	if(!buildTrianglePipelineBootstrapConfig(positionStream, colorStream, topology, primitiveCount, renderArea, &config, fragmentConfig, indexData, indexType, baseVertex, frontFaceCounterClockwise, pointSize))
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

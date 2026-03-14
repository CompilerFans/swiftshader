#include "TrianglePipelineBootstrap.hpp"

#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RasterBootstrap.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
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

uint32_t floatFormatComponentCount(VkFormat format)
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

struct PackedBootstrapVertex
{
	float position[3] = {};
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float texCoord[2] = {};
	float normal[3] = { 0.0f, 0.0f, 1.0f };
};

bool streamAliasesPositionBinding(const sw::Stream *stream, const sw::Stream &positionStream)
{
	return stream != nullptr &&
	       stream->buffer != nullptr &&
	       stream->binding == positionStream.binding &&
	       stream->inputRate == positionStream.inputRate &&
	       stream->vertexStride == positionStream.vertexStride;
}

bool resolveSourceIndex(uint32_t vertexIndex, const void *indexData, VkIndexType indexType, int32_t baseVertex, uint32_t *sourceIndex)
{
	if(sourceIndex == nullptr)
	{
		return false;
	}

	int64_t resolvedIndex = vertexIndex;
	if(indexData != nullptr)
	{
		switch(indexType)
		{
		case VK_INDEX_TYPE_UINT16:
			resolvedIndex = static_cast<int64_t>(static_cast<const uint16_t *>(indexData)[vertexIndex]) + baseVertex;
			break;
		case VK_INDEX_TYPE_UINT32:
			resolvedIndex = static_cast<int64_t>(static_cast<const uint32_t *>(indexData)[vertexIndex]) + baseVertex;
			break;
		case VK_INDEX_TYPE_UINT8_EXT:
			resolvedIndex = static_cast<int64_t>(static_cast<const uint8_t *>(indexData)[vertexIndex]) + baseVertex;
			break;
		default:
			return false;
		}
	}

	if(resolvedIndex < 0)
	{
		return false;
	}

	*sourceIndex = static_cast<uint32_t>(resolvedIndex);
	return true;
}

bool readStreamFloatComponents(const sw::Stream &stream, uint32_t sourceIndex, uint32_t maxComponents, float *components)
{
	if(stream.buffer == nullptr || components == nullptr || maxComponents == 0)
	{
		return false;
	}

	const uint32_t componentCount = floatFormatComponentCount(stream.format);
	if(componentCount == 0 || componentCount > maxComponents)
	{
		return false;
	}

	const auto *source = reinterpret_cast<const float *>(static_cast<const uint8_t *>(stream.buffer) +
	                                                     static_cast<size_t>(sourceIndex) * stream.vertexStride);
	for(uint32_t i = 0; i < componentCount; i++)
	{
		components[i] = source[i];
	}
	return true;
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

bool shouldTraceBootstrap()
{
	const char *value = std::getenv("SWIFTSHADER_CUDA_TRACE_TRIANGLE_BOOTSTRAP");
	return value != nullptr && value[0] != '\0';
}

void traceBootstrapFailure(const char *reason, const sw::Stream &positionStream, VkPrimitiveTopology topology, uint32_t primitiveCount, VkIndexType indexType, const void *indexData)
{
	if(!shouldTraceBootstrap())
	{
		return;
	}

	std::fprintf(stderr,
	             "[cuda-bootstrap] TrianglePipelineBootstrap skipped: %s (topology=%u, primitives=%u, pos.buffer=%p, pos.format=%u, pos.rate=%u, pos.stride=%u, pos.binding=%u, indexType=%u, indexData=%p)\n",
	             reason,
	             static_cast<unsigned int>(topology),
	             primitiveCount,
	             positionStream.buffer,
	             static_cast<unsigned int>(positionStream.format),
	             static_cast<unsigned int>(positionStream.inputRate),
	             positionStream.vertexStride,
	             positionStream.binding,
	             static_cast<unsigned int>(indexType),
	             indexData);
}

}  // namespace

bool buildTrianglePipelineBootstrapConfig(const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, TrianglePipelineBootstrapConfig *config, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise, float pointSize, const sw::Stream *texCoordStream, const sw::Stream *normalStream)
{
	if(config == nullptr || positionStream.buffer == nullptr)
	{
		traceBootstrapFailure(config == nullptr ? "null config" : "null position buffer", positionStream, topology, primitiveCount, indexType, indexData);
		return false;
	}
	if((topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST && topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN && topology != VK_PRIMITIVE_TOPOLOGY_LINE_LIST && topology != VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) || primitiveCount == 0)
	{
		traceBootstrapFailure(primitiveCount == 0 ? "zero primitive count" : "unsupported topology", positionStream, topology, primitiveCount, indexType, indexData);
		return false;
	}
	if(positionStream.inputRate != VK_VERTEX_INPUT_RATE_VERTEX || positionStream.vertexStride == 0)
	{
		traceBootstrapFailure(positionStream.inputRate != VK_VERTEX_INPUT_RATE_VERTEX ? "position stream is not per-vertex" : "position stream stride is zero",
		                      positionStream, topology, primitiveCount, indexType, indexData);
		return false;
	}

	const uint32_t componentCount = floatFormatComponentCount(positionStream.format);
	if(componentCount == 0)
	{
		traceBootstrapFailure("unsupported position format", positionStream, topology, primitiveCount, indexType, indexData);
		return false;
	}

	config->width = renderArea.extent.width;
	config->height = renderArea.extent.height;
	config->topology = topology;
	config->pointSize = pointSize;
	config->vertexCount = topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? primitiveCount : ((topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST) ? primitiveCount * 2u : ((topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN) ? primitiveCount + (topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ? 1u : 2u) : primitiveCount * 3u));
	config->frontFaceCounterClockwise = frontFaceCounterClockwise;

	const bool needsPackedAuxiliaryVertexData =
	    (colorStream && colorStream->buffer != nullptr && !streamAliasesPositionBinding(colorStream, positionStream)) ||
	    (texCoordStream && texCoordStream->buffer != nullptr && !streamAliasesPositionBinding(texCoordStream, positionStream)) ||
	    (normalStream && normalStream->buffer != nullptr && !streamAliasesPositionBinding(normalStream, positionStream));

	if(needsPackedAuxiliaryVertexData || (normalStream && normalStream->buffer != nullptr))
	{
		config->binding.vertexStride = sizeof(PackedBootstrapVertex);
		config->binding.positionOffset = offsetof(PackedBootstrapVertex, position);
		config->binding.positionComponentCount = std::min(componentCount, 3u);
		config->binding.colorOffset = offsetof(PackedBootstrapVertex, color);
		config->binding.colorComponentCount = (colorStream && colorStream->buffer != nullptr) ? std::min(floatFormatComponentCount(colorStream->format), 4u) : 0u;
		config->binding.texCoordOffset = offsetof(PackedBootstrapVertex, texCoord);
		config->binding.texCoordComponentCount = (texCoordStream && texCoordStream->buffer != nullptr) ? std::min(floatFormatComponentCount(texCoordStream->format), 2u) : 0u;
		config->binding.normalOffset = offsetof(PackedBootstrapVertex, normal);
		config->binding.normalComponentCount = (normalStream && normalStream->buffer != nullptr) ? std::min(floatFormatComponentCount(normalStream->format), 3u) : 0u;

		config->rawVertexData.resize(static_cast<size_t>(config->binding.vertexStride) * config->vertexCount);
		for(uint32_t vertexIndex = 0; vertexIndex < config->vertexCount; vertexIndex++)
		{
			uint32_t sourceIndex = 0;
			if(!resolveSourceIndex(vertexIndex, indexData, indexType, baseVertex, &sourceIndex))
			{
				return false;
			}

			PackedBootstrapVertex packedVertex = {};
			if(!readStreamFloatComponents(positionStream, sourceIndex, 3u, packedVertex.position))
			{
				return false;
			}
			if(colorStream && colorStream->buffer != nullptr)
			{
				if(!readStreamFloatComponents(*colorStream, sourceIndex, 4u, packedVertex.color))
				{
					return false;
				}
			}
			if(texCoordStream && texCoordStream->buffer != nullptr)
			{
				if(!readStreamFloatComponents(*texCoordStream, sourceIndex, 2u, packedVertex.texCoord))
				{
					return false;
				}
			}
			if(normalStream && normalStream->buffer != nullptr)
			{
				if(!readStreamFloatComponents(*normalStream, sourceIndex, 3u, packedVertex.normal))
				{
					return false;
				}
			}

			std::memcpy(config->rawVertexData.data() + static_cast<size_t>(vertexIndex) * config->binding.vertexStride,
			            &packedVertex,
			            sizeof(packedVertex));
		}
	}
	else
	{
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

		config->binding.vertexStride = positionStream.vertexStride;
		config->binding.positionOffset = 0;
		config->binding.positionComponentCount = componentCount;
		config->binding.colorOffset = 0;
		config->binding.colorComponentCount = 0;
		if(streamAliasesPositionBinding(colorStream, positionStream))
		{
			const uint32_t colorComponentCount = floatFormatComponentCount(colorStream->format);
			auto positionBase = static_cast<const uint8_t *>(positionStream.buffer);
			auto colorBase = static_cast<const uint8_t *>(colorStream->buffer);
			if(colorComponentCount >= 3 && colorBase >= positionBase)
			{
				config->binding.colorOffset = static_cast<uint32_t>(colorBase - positionBase);
				config->binding.colorComponentCount = colorComponentCount;
			}
		}
		config->binding.texCoordOffset = 0;
		config->binding.texCoordComponentCount = 0;
		if(streamAliasesPositionBinding(texCoordStream, positionStream))
		{
			const uint32_t texCoordComponentCount = floatFormatComponentCount(texCoordStream->format);
			auto positionBase = static_cast<const uint8_t *>(positionStream.buffer);
			auto texCoordBase = static_cast<const uint8_t *>(texCoordStream->buffer);
			if(texCoordComponentCount >= 2 && texCoordBase >= positionBase)
			{
				config->binding.texCoordOffset = static_cast<uint32_t>(texCoordBase - positionBase);
				config->binding.texCoordComponentCount = texCoordComponentCount;
			}
		}
		config->binding.normalOffset = 0;
		config->binding.normalComponentCount = 0;
		if(streamAliasesPositionBinding(normalStream, positionStream))
		{
			const uint32_t normalComponentCount = floatFormatComponentCount(normalStream->format);
			auto positionBase = static_cast<const uint8_t *>(positionStream.buffer);
			auto normalBase = static_cast<const uint8_t *>(normalStream->buffer);
			if(normalComponentCount >= 3 && normalBase >= positionBase)
			{
				config->binding.normalOffset = static_cast<uint32_t>(normalBase - positionBase);
				config->binding.normalComponentCount = normalComponentCount;
			}
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
		GraphicsBootstrapShaderConfig shaderConfig = {};
		shaderConfig.pointSize = config.pointSize;
		GraphicsBootstrapRuntimeConfig runtimeConfig = config.runtimeConfig;

		if(!config.rawVertexData.empty() && config.vertexCount != 0)
		{
			if(!runGraphicsBootstrap(runtime, config.rawVertexData, config.vertexCount, config.binding, shaderConfig, runtimeConfig, &vsOutputs))
			{
				return false;
			}
		}
		else
		{
			std::vector<GraphicsBootstrapVertexInput> vertexInputs(config.vertices.begin(), config.vertices.end());
			if(!runGraphicsBootstrap(runtime, vertexInputs, shaderConfig, runtimeConfig, &vsOutputs))
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
			float texCoordU = 0.0f;
			float texCoordV = 0.0f;
			if(config.binding.texCoordComponentCount != 0)
			{
				const float *texCoord = reinterpret_cast<const float *>(config.rawVertexData.data() + i * config.binding.vertexStride + config.binding.texCoordOffset);
				texCoordU = texCoord[0];
				texCoordV = config.binding.texCoordComponentCount > 1 ? texCoord[1] : 0.0f;
			}
			if(config.runtimeConfig.vertexMode == GraphicsBootstrapRuntimeConfig::VertexMode::UniformTransformLighting)
			{
				float normalX = 0.0f;
				float normalY = 0.0f;
				float normalZ = 1.0f;
				if(config.binding.normalComponentCount != 0)
				{
					const float *normal = reinterpret_cast<const float *>(config.rawVertexData.data() + i * config.binding.vertexStride + config.binding.normalOffset);
					normalX = normal[0];
					normalY = config.binding.normalComponentCount > 1 ? normal[1] : 0.0f;
					normalZ = config.binding.normalComponentCount > 2 ? normal[2] : 1.0f;
				}

				auto mulMat4Vec4 = [](const float *matrix, float x, float y, float zValue, float wValue, float &outX, float &outY, float &outZ, float &outW) {
					outX = matrix[0] * x + matrix[4] * y + matrix[8] * zValue + matrix[12] * wValue;
					outY = matrix[1] * x + matrix[5] * y + matrix[9] * zValue + matrix[13] * wValue;
					outZ = matrix[2] * x + matrix[6] * y + matrix[10] * zValue + matrix[14] * wValue;
					outW = matrix[3] * x + matrix[7] * y + matrix[11] * zValue + matrix[15] * wValue;
				};
				auto mulMat3Vec3 = [](const float *matrix, float x, float y, float zValue, float &outX, float &outY, float &outZ) {
					outX = matrix[0] * x + matrix[4] * y + matrix[8] * zValue;
					outY = matrix[1] * x + matrix[5] * y + matrix[9] * zValue;
					outZ = matrix[2] * x + matrix[6] * y + matrix[10] * zValue;
				};

				float clipX = 0.0f;
				float clipY = 0.0f;
				float clipZ = 0.0f;
				float clipW = 1.0f;
				mulMat4Vec4(config.runtimeConfig.modelViewProjection, position[0], position[1], z, 1.0f, clipX, clipY, clipZ, clipW);

				float viewX = 0.0f;
				float viewY = 0.0f;
				float viewZ = 0.0f;
				float viewW = 1.0f;
				mulMat4Vec4(config.runtimeConfig.modelView, position[0], position[1], z, 1.0f, viewX, viewY, viewZ, viewW);
				mulMat3Vec3(config.runtimeConfig.normalMatrix, normalX, normalY, normalZ, normalX, normalY, normalZ);

				const float reciprocalW = std::fabs(viewW) > 1.0e-20f ? 1.0f / viewW : 1.0f;
				float lightDirX = 2.0f - viewX * reciprocalW;
				float lightDirY = 2.0f - viewY * reciprocalW;
				float lightDirZ = 20.0f - viewZ * reciprocalW;
				const float lightLength = std::sqrt(lightDirX * lightDirX + lightDirY * lightDirY + lightDirZ * lightDirZ);
				if(lightLength > 1.0e-20f)
				{
					lightDirX /= lightLength;
					lightDirY /= lightLength;
					lightDirZ /= lightLength;
				}
				const float diffuse = std::max(normalX * lightDirX + normalY * lightDirY + normalZ * lightDirZ, 0.0f);
				vsOutputs[i] = { clipX,
				                 clipY,
				                 clipZ,
				                 clipW,
				                 config.pointSize,
				                 colorR * diffuse,
				                 colorG * diffuse,
				                 colorB * diffuse,
				                 1.0f,
				                 texCoordU,
				                 texCoordV };
				continue;
			}

			vsOutputs[i] = { position[0] + config.runtimeConfig.offsetX,
			                 position[1] + config.runtimeConfig.offsetY,
			                 z + config.runtimeConfig.offsetZ,
			                 1.0f,
			                 config.pointSize,
			                 colorR,
			                 colorG,
			                 colorB,
			                 colorA,
			                 texCoordU,
			                 texCoordV };
		}
	}
	else
	{
		vsOutputs.resize(config.vertices.size());
		for(size_t i = 0; i < config.vertices.size(); i++)
		{
			vsOutputs[i] = { config.vertices[i].x, config.vertices[i].y, config.vertices[i].z, 1.0f, config.pointSize, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
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
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
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
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::Texture2DColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
			{
				fragmentConfig.vertexTexCoord0U = fanVertices[0].u;
				fragmentConfig.vertexTexCoord0V = fanVertices[0].v;
				fragmentConfig.vertexTexCoord1U = fanVertices[1].u;
				fragmentConfig.vertexTexCoord1V = fanVertices[1].v;
				fragmentConfig.vertexTexCoord2U = fanVertices[2].u;
				fragmentConfig.vertexTexCoord2V = fanVertices[2].v;
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
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
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
			if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::Texture2DColor ||
			   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
			{
				fragmentConfig.vertexTexCoord0U = stripVertices[0].u;
				fragmentConfig.vertexTexCoord0V = stripVertices[0].v;
				fragmentConfig.vertexTexCoord1U = stripVertices[1].u;
				fragmentConfig.vertexTexCoord1V = stripVertices[1].v;
				fragmentConfig.vertexTexCoord2U = stripVertices[2].u;
				fragmentConfig.vertexTexCoord2V = stripVertices[2].v;
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
		if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColor ||
		   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth ||
		   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::FlatInterpolatedColor ||
		   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
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
		if(fragmentConfig.shaderKind == FragmentBootstrapShaderKind::Texture2DColor ||
		   fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
		{
			fragmentConfig.vertexTexCoord0U = vsOutputs[primitiveIndex * 3 + 0].u;
			fragmentConfig.vertexTexCoord0V = vsOutputs[primitiveIndex * 3 + 0].v;
			fragmentConfig.vertexTexCoord1U = vsOutputs[primitiveIndex * 3 + 1].u;
			fragmentConfig.vertexTexCoord1V = vsOutputs[primitiveIndex * 3 + 1].v;
			fragmentConfig.vertexTexCoord2U = vsOutputs[primitiveIndex * 3 + 2].u;
			fragmentConfig.vertexTexCoord2V = vsOutputs[primitiveIndex * 3 + 2].v;
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

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, std::vector<uint8_t> *colorBuffer, const FragmentBootstrapConfig *fragmentConfig, const void *indexData, VkIndexType indexType, int32_t baseVertex, bool frontFaceCounterClockwise, float pointSize, const sw::Stream *texCoordStream, const sw::Stream *normalStream)
{
	TrianglePipelineBootstrapConfig config = {};
	if(!buildTrianglePipelineBootstrapConfig(positionStream, colorStream, topology, primitiveCount, renderArea, &config, fragmentConfig, indexData, indexType, baseVertex, frontFaceCounterClockwise, pointSize, texCoordStream, normalStream))
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

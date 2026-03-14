#ifndef SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_
#define SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace backend {

struct GraphicsBootstrapVertexInput
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float colorR = 1.0f;
	float colorG = 1.0f;
	float colorB = 1.0f;
	float colorA = 1.0f;
	float u = 0.0f;
	float v = 0.0f;
	float normalX = 0.0f;
	float normalY = 0.0f;
	float normalZ = 1.0f;
};

struct GraphicsBootstrapVertexOutput
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;
	float pointSize = 64.0f;
	float colorR = 1.0f;
	float colorG = 1.0f;
	float colorB = 1.0f;
	float colorA = 1.0f;
	float u = 0.0f;
	float v = 0.0f;
};

struct GraphicsBootstrapShaderConfig
{
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
	float pointSize = 64.0f;
	float vertexIndexScaleX = 0.0f;
	float instanceIndexScaleY = 0.0f;
};

struct GraphicsBootstrapRuntimeConfig
{
	enum class VertexMode : uint32_t
	{
		Passthrough = 0,
		UniformTransformLighting = 1,
	};

	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
	uint32_t instanceIndex = 0;
	VertexMode vertexMode = VertexMode::Passthrough;
	float modelView[16] = {};
	float modelViewProjection[16] = {};
	float normalMatrix[12] = {};
};

struct GraphicsBootstrapBindingConfig
{
	uint32_t vertexStride = sizeof(GraphicsBootstrapVertexInput);
	uint32_t positionOffset = 0;
	uint32_t positionComponentCount = 3;
	uint32_t colorOffset = offsetof(GraphicsBootstrapVertexInput, colorR);
	uint32_t colorComponentCount = 4;
	uint32_t texCoordOffset = offsetof(GraphicsBootstrapVertexInput, u);
	uint32_t texCoordComponentCount = 2;
	uint32_t normalOffset = offsetof(GraphicsBootstrapVertexInput, normalX);
	uint32_t normalComponentCount = 3;
};

std::string graphicsBootstrapCudaSource(const GraphicsBootstrapShaderConfig &config = {});
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<uint8_t> &rawVertexData, uint32_t vertexCount, const GraphicsBootstrapBindingConfig &bindingConfig, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs);
void launchGraphicsBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

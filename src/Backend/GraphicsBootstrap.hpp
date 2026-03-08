#ifndef SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_
#define SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace backend {

struct GraphicsBootstrapVertexInput
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct GraphicsBootstrapVertexOutput
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;
};

struct GraphicsBootstrapShaderConfig
{
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
	float vertexIndexScaleX = 0.0f;
};

struct GraphicsBootstrapRuntimeConfig
{
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
};

struct GraphicsBootstrapBindingConfig
{
	uint32_t vertexStride = sizeof(GraphicsBootstrapVertexInput);
	uint32_t positionOffset = 0;
};

std::string graphicsBootstrapCudaSource(const GraphicsBootstrapShaderConfig &config = {});
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs);
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<uint8_t> &rawVertexData, uint32_t vertexCount, const GraphicsBootstrapBindingConfig &bindingConfig, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs);
void launchGraphicsBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

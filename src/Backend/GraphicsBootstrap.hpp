#ifndef SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_
#define SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

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

std::string graphicsBootstrapCudaSource();
bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, std::vector<GraphicsBootstrapVertexOutput> *outputs);
void launchGraphicsBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

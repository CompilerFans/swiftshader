#ifndef SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_
#define SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstdint>
#include <vector>

namespace backend {

struct TrianglePipelineBootstrapConfig
{
	uint32_t width = 64;
	uint32_t height = 64;
	float colorR = 0.0f;
	float colorG = 1.0f;
	float colorB = 0.0f;
	float colorA = 1.0f;
};

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const TrianglePipelineBootstrapConfig &config, std::vector<uint8_t> *colorBuffer);
bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, std::vector<uint8_t> *colorBuffer);
void launchTrianglePipelineBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

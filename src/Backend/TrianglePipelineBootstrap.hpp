#ifndef SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_
#define SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

#include "Device/Stream.hpp"
#include "FragmentBootstrap.hpp"
#include "GraphicsBootstrap.hpp"
#include "RuntimeAPI.hpp"

#include <array>
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
	FragmentBootstrapConfig fragmentConfig = {};
	std::vector<uint8_t> rawVertexData;
	uint32_t vertexCount = 0;
	GraphicsBootstrapBindingConfig binding = {};
	std::array<GraphicsBootstrapVertexInput, 3> vertices = {{
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	}};
};

bool buildTrianglePipelineBootstrapConfig(const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, TrianglePipelineBootstrapConfig *config, const FragmentBootstrapConfig *fragmentConfig = nullptr);
bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const TrianglePipelineBootstrapConfig &config, std::vector<uint8_t> *colorBuffer);
bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, const sw::Stream &positionStream, const sw::Stream *colorStream, VkPrimitiveTopology topology, uint32_t primitiveCount, const VkRect2D &renderArea, std::vector<uint8_t> *colorBuffer, const FragmentBootstrapConfig *fragmentConfig = nullptr);
bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, std::vector<uint8_t> *colorBuffer);
void launchTrianglePipelineBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

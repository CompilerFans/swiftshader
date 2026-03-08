#ifndef SWIFTSHADER_RASTER_BOOTSTRAP_HPP_
#define SWIFTSHADER_RASTER_BOOTSTRAP_HPP_

#include "FragmentBootstrap.hpp"
#include "RuntimeAPI.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace backend {

struct RasterBootstrapVertex
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;
};

struct RasterBootstrapConfig
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t reservedTileWidth = 0;
	uint32_t reservedQuadWidth = 0;
};

struct RasterBootstrapOutput
{
	bool valid = false;
	uint32_t bboxMinX = 0;
	uint32_t bboxMinY = 0;
	uint32_t bboxMaxX = 0;
	uint32_t bboxMaxY = 0;
	std::vector<FragmentBootstrapInvocation> invocations;
};

std::string rasterBootstrapCudaSource();
RasterBootstrapOutput rasterBootstrapCpuReference(const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &config);
bool runRasterBootstrap(RuntimeAPI &runtime, const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &config, RasterBootstrapOutput *output);
bool runRasterFragmentBootstrap(RuntimeAPI &runtime, const std::array<RasterBootstrapVertex, 3> &triangle, const RasterBootstrapConfig &rasterConfig, const FragmentBootstrapConfig &fragmentConfig, std::vector<uint8_t> *colorBuffer);
void launchRasterBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_RASTER_BOOTSTRAP_HPP_

#ifndef SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_
#define SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstdint>
#include <vector>

namespace backend {

bool runTrianglePipelineBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, std::vector<uint8_t> *colorBuffer);
void launchTrianglePipelineBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_TRIANGLE_PIPELINE_BOOTSTRAP_HPP_

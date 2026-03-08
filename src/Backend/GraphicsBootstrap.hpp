#ifndef SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_
#define SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <string>

namespace backend {

std::string graphicsBootstrapCudaSource();
void launchGraphicsBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_BOOTSTRAP_HPP_

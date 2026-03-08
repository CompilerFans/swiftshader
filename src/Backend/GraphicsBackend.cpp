#include "GraphicsBackend.hpp"

#include "Backend/BackendConfig.hpp"

namespace backend {

GraphicsBootstrapMode defaultGraphicsBootstrapMode()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return GraphicsBootstrapMode::CustomWithCpuGraphicsFallback;
#else
	return GraphicsBootstrapMode::CpuOnly;
#endif
}

}  // namespace backend

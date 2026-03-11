#include "GraphicsBackend.hpp"

#include "Backend/BackendConfig.hpp"

namespace backend {
namespace {

ExecutionBackendCapture gCapture = {};

}  // namespace

GraphicsBootstrapMode defaultGraphicsBootstrapMode()
{
#if SWIFTSHADER_ENABLE_GPU_BACKEND
	return GraphicsBootstrapMode::GpuWithCpuGraphicsFallback;
#else
	return GraphicsBootstrapMode::CpuOnly;
#endif
}

void resetExecutionBackendCapture()
{
	gCapture = {};
}

const ExecutionBackendCapture &lastExecutionBackendCapture()
{
	return gCapture;
}

}  // namespace backend

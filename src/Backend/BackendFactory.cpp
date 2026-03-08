#include "BackendFactory.hpp"
#include "BackendConfig.hpp"
#include "FakeRuntimeAPI.hpp"

namespace backend {

BackendKind defaultBackendKind()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return BackendKind::CUSTOM_GPU;
#else
	return BackendKind::CPU;
#endif
}

std::unique_ptr<RuntimeAPI> createRuntimeAPI()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return std::make_unique<FakeRuntimeAPI>();
#else
	return nullptr;
#endif
}

}  // namespace backend

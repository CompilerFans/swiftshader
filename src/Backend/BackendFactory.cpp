#include "BackendFactory.hpp"
#include "BackendConfig.hpp"
#include "FakeRuntimeAPI.hpp"
#include "PresentAdapter.hpp"

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

std::unique_ptr<PresentAdapter> createPresentAdapter()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return createCustomPresentAdapter();
#else
	return createFallbackPresentAdapter();
#endif
}


}  // namespace backend

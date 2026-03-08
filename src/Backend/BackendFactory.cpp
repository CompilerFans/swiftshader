#include "BackendFactory.hpp"
#include "BackendConfig.hpp"

namespace backend {

BackendKind defaultBackendKind()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return BackendKind::CUSTOM_GPU;
#else
	return BackendKind::CPU;
#endif
}

}  // namespace backend

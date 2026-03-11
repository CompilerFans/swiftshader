#include "BackendFactory.hpp"

#include "GraphicsBackend.hpp"

namespace backend {

std::unique_ptr<ExecutionBackend> createExecutionBackend(vk::Device *device)
{
	switch(defaultBackendKind())
	{
	case BackendKind::CPU:
		return createCpuExecutionBackend(device);
	case BackendKind::GPU:
		return createGpuExecutionBackend(device);
	default:
		return createCpuExecutionBackend(device);
	}
}

}  // namespace backend

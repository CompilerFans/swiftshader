#include "GraphicsBackend.hpp"

namespace backend {

std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device)
{
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedCustomFactory = true;
	return createCpuExecutionBackend(device);
}

}  // namespace backend

#include "GraphicsBackend.hpp"

namespace backend {

std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device)
{
	return createCpuExecutionBackend(device);
}

}  // namespace backend

#ifndef SWIFTSHADER_GRAPHICS_BACKEND_HPP_
#define SWIFTSHADER_GRAPHICS_BACKEND_HPP_

#include "Backend/ExecutionBackend.hpp"

#include <memory>

namespace sw {
class Renderer;
}

namespace vk {
class Device;
}

namespace backend {

enum class GraphicsBootstrapMode
{
	CpuOnly,
	CustomWithCpuGraphicsFallback,
};

class GraphicsBackend
{
public:
	virtual ~GraphicsBackend() = default;

	virtual sw::Renderer *renderer() = 0;
	virtual void synchronize() = 0;
};

GraphicsBootstrapMode defaultGraphicsBootstrapMode();
std::unique_ptr<ExecutionBackend> createCpuExecutionBackend(vk::Device *device);
std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_BACKEND_HPP_

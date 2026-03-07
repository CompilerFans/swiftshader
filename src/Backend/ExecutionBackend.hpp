#ifndef SWIFTSHADER_EXECUTION_BACKEND_HPP_
#define SWIFTSHADER_EXECUTION_BACKEND_HPP_

#include <memory>

namespace sw {
class CountedEvent;
}

namespace vk {
class Device;
struct SubmitInfo;
}

namespace backend {

enum class BackendKind
{
	CPU,
	CUSTOM_GPU,
};

class ExecutionBackend
{
public:
	virtual ~ExecutionBackend() = default;

	virtual void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) = 0;
	virtual void synchronize() = 0;
};

}  // namespace backend

#endif  // SWIFTSHADER_EXECUTION_BACKEND_HPP_

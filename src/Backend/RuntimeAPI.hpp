#ifndef SWIFTSHADER_RUNTIME_API_HPP_
#define SWIFTSHADER_RUNTIME_API_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

namespace backend {

struct ModuleHandle
{
	uint64_t id = 0;

	bool valid() const
	{
		return id != 0;
	}
};

struct LaunchRecord
{
	ModuleHandle module = {};
	uint32_t baseGroupX = 0;
	uint32_t baseGroupY = 0;
	uint32_t baseGroupZ = 0;
	uint32_t groupCountX = 0;
	uint32_t groupCountY = 0;
	uint32_t groupCountZ = 0;
	size_t bindingCount = 0;
	size_t argumentWords = 0;
};

class RuntimeAPI
{
public:
	virtual ~RuntimeAPI() = default;

	virtual ModuleHandle createModule(const std::string &sourceOrIR) = 0;
	virtual void launch(ModuleHandle module, const LaunchRecord &record) = 0;
};

}  // namespace backend

#endif  // SWIFTSHADER_RUNTIME_API_HPP_

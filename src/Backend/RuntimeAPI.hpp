#ifndef SWIFTSHADER_RUNTIME_API_HPP_
#define SWIFTSHADER_RUNTIME_API_HPP_

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

class RuntimeAPI
{
public:
	virtual ~RuntimeAPI() = default;

	virtual ModuleHandle createModule(const std::string &sourceOrIR) = 0;
};

}  // namespace backend

#endif  // SWIFTSHADER_RUNTIME_API_HPP_

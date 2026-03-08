#ifndef SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_
#define SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_

#include "Backend/RuntimeAPI.hpp"

#include <memory>
#include <string>

namespace sw {
struct ParsedSpirvInfo;
}

namespace backend {

struct ComputeDispatchInfo
{
	uint32_t baseGroupX = 0;
	uint32_t baseGroupY = 0;
	uint32_t baseGroupZ = 0;
	uint32_t groupCountX = 0;
	uint32_t groupCountY = 0;
	uint32_t groupCountZ = 0;
	size_t bindingCount = 0;
	size_t argumentWords = 0;
};

class ComputeExecutable
{
public:
	static std::shared_ptr<ComputeExecutable> create(const sw::ParsedSpirvInfo &parsed);

	bool valid() const { return !source.empty(); }
	const std::string &sourceText() const { return source; }

	ModuleHandle ensureModule(RuntimeAPI &runtime);
	void dispatch(RuntimeAPI &runtime, const ComputeDispatchInfo &dispatchInfo);

private:
	explicit ComputeExecutable(std::string sourceText) : source(std::move(sourceText)) {}

	std::string source;
	ModuleHandle module = {};
};

}  // namespace backend

#endif  // SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_

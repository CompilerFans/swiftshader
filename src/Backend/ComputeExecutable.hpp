#ifndef SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_
#define SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_

#include "Backend/RuntimeAPI.hpp"

#include <memory>
#include <string>

namespace sw {
struct ParsedSpirvInfo;
class SpirvBinary;
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
	uint32_t blockCountX = 1;
	uint32_t blockCountY = 1;
	uint32_t blockCountZ = 1;
	size_t bindingCount = 0;
	size_t argumentWords = 0;

	// Optional buffer-to-buffer dispatch parameters used by minimal CUDA compute support.
	// When unset, the dispatch will still record the launch dimensions, but may not execute
	// any meaningful compute work on hardware-backed runtimes.
	const void *inputBuffer = nullptr;
	void *outputBuffer = nullptr;
	size_t inputSizeInBytes = 0;
	size_t outputSizeInBytes = 0;
};

class ComputeExecutable
{
public:
	enum class Kind
	{
		Unsupported,
		Empty,
		BufferMemcpy,
	};

	static std::shared_ptr<ComputeExecutable> create(const sw::ParsedSpirvInfo &parsed, const sw::SpirvBinary &spirv);

	bool valid() const { return !source.empty(); }
	const std::string &sourceText() const { return source; }
	Kind kind() const { return executableKind; }

	ModuleHandle ensureModule(RuntimeAPI &runtime);
	void dispatch(RuntimeAPI &runtime, const ComputeDispatchInfo &dispatchInfo);

private:
	explicit ComputeExecutable(Kind kind, std::string sourceText)
	    : executableKind(kind)
	    , source(std::move(sourceText))
	{}

	Kind executableKind = Kind::Unsupported;
	std::string source;
	ModuleHandle module = {};
};

}  // namespace backend

#endif  // SWIFTSHADER_COMPUTE_EXECUTABLE_HPP_

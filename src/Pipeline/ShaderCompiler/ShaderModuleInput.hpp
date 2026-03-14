#ifndef SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_MODULE_INPUT_HPP_
#define SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_MODULE_INPUT_HPP_

#include "SpirvBinary.hpp"

#include <string>

namespace sw {

class ShaderModuleInput
{
public:
	enum class Kind
	{
		SpirvBinary,
		SpirvAssemblyText,
	};

	static ShaderModuleInput fromBinary(std::string entryPointName, SpirvBinary spirv)
	{
		return ShaderModuleInput(Kind::SpirvBinary, std::move(entryPointName), std::move(spirv), {});
	}

	static ShaderModuleInput fromAssembly(std::string entryPointName, std::string spirvAssembly)
	{
		return ShaderModuleInput(Kind::SpirvAssemblyText, std::move(entryPointName), {}, std::move(spirvAssembly));
	}

	Kind kind() const { return inputKind; }
	const std::string &entryPoint() const { return entryPointName; }
	const SpirvBinary &spirvBinary() const { return spirv; }
	const std::string &spirvAssembly() const { return assembly; }

private:
	ShaderModuleInput(Kind kind, std::string entryPoint, SpirvBinary binary, std::string assemblyText)
	    : inputKind(kind)
	    , entryPointName(std::move(entryPoint))
	    , spirv(std::move(binary))
	    , assembly(std::move(assemblyText))
	{}

	Kind inputKind;
	std::string entryPointName;
	SpirvBinary spirv;
	std::string assembly;
};

}  // namespace sw

#endif  // SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_MODULE_INPUT_HPP_

#include "ShaderCompilerTool.hpp"

#include "CodegenTarget.hpp"
#include "ShaderCompiler.hpp"
#include "ShaderCompilerAnalysis.hpp"
#include "ShaderModuleInput.hpp"

#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

namespace sw {
namespace {

struct Options
{
	std::string stage;
	std::string entryPoint;
	std::string inputFormat;
	std::string outputFormat;
	std::string inputPath;
};

bool parseArgs(const std::vector<std::string> &args, Options *options, std::ostream &err)
{
	if(options == nullptr)
	{
		return false;
	}

	for(size_t i = 0; i < args.size();)
	{
		if(i + 1 >= args.size())
		{
			err << "missing value for argument: " << args[i] << "\n";
			return false;
		}

		const std::string &key = args[i];
		const std::string &value = args[i + 1];
		if(key == "--stage")
		{
			options->stage = value;
		}
		else if(key == "--entry-point")
		{
			options->entryPoint = value;
		}
		else if(key == "--input-format")
		{
			options->inputFormat = value;
		}
		else if(key == "--output-format")
		{
			options->outputFormat = value;
		}
		else if(key == "--input")
		{
			options->inputPath = value;
		}
		else
		{
			err << "unknown argument: " << key << "\n";
			return false;
		}
		i += 2;
	}

	if(options->stage.empty() || options->entryPoint.empty() || options->inputFormat.empty() ||
	   options->outputFormat.empty() || options->inputPath.empty())
	{
		err << "missing required arguments\n";
		return false;
	}

	return true;
}

ShaderCompilerAnalysisContext defaultToolContext()
{
	ShaderCompilerAnalysisContext context = {};
	context.descriptorSetCount = 1;
	context.dynamicOffsetCount = 0;
	context.pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
	context.queryDescriptorBindingInfo = [](const void *, uint32_t, uint32_t binding, ShaderDescriptorBindingInfo *bindingInfo) {
		if(bindingInfo == nullptr)
		{
			return false;
		}

		// Minimal offline default for smoke usage. Rich descriptor-layout driven tooling can layer on later.
		if(binding == 0)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		if(binding == 1)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		return false;
	};
	return context;
}

bool readTextFile(const std::string &path, std::string *text)
{
	if(text == nullptr)
	{
		return false;
	}

	std::ifstream stream(path);
	if(!stream.is_open())
	{
		return false;
	}

	*text = std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	return true;
}

bool readBinaryFile(const std::string &path, SpirvBinary *binary, std::ostream &err)
{
	if(binary == nullptr)
	{
		return false;
	}

	std::ifstream stream(path, std::ios::binary);
	if(!stream.is_open())
	{
		return false;
	}

	std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if(bytes.empty() || (bytes.size() % sizeof(uint32_t)) != 0)
	{
		err << "invalid spirv binary size\n";
		return false;
	}

	std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
	std::memcpy(words.data(), bytes.data(), bytes.size());
	*binary = SpirvBinary(words.data(), static_cast<uint32_t>(words.size()));
	return true;
}

}  // namespace

int runShaderCompilerTool(const std::vector<std::string> &args, std::ostream &out, std::ostream &err)
{
	Options options = {};
	if(!parseArgs(args, &options, err))
	{
		return 1;
	}

	CodegenTarget target = CodegenTarget::CudaLikeSource;
	if(options.outputFormat == "cuda")
	{
		target = CodegenTarget::CudaLikeSource;
	}
	else if(options.outputFormat == "llvm")
	{
		target = CodegenTarget::LlvmIR;
	}
	else
	{
		err << "unsupported output format: " << options.outputFormat << "\n";
		return 1;
	}

	ShaderModuleInput input = ShaderModuleInput::fromAssembly(options.entryPoint, "");
	if(options.inputFormat == "spvasm")
	{
		std::string assembly;
		if(!readTextFile(options.inputPath, &assembly))
		{
			err << "failed to read input file: " << options.inputPath << "\n";
			return 1;
		}
		input = ShaderModuleInput::fromAssembly(options.entryPoint, std::move(assembly));
	}
	else if(options.inputFormat == "spvbin")
	{
		SpirvBinary binary;
		if(!readBinaryFile(options.inputPath, &binary, err))
		{
			err << "failed to read input file: " << options.inputPath << "\n";
			return 1;
		}
		input = ShaderModuleInput::fromBinary(options.entryPoint, std::move(binary));
	}
	else
	{
		err << "unsupported input format: " << options.inputFormat << "\n";
		return 1;
	}

	ShaderCompiler compiler;
	ShaderCompilerOutput result = {};
	if(options.stage == "fragment")
	{
		result = compiler.compileGraphicsFragment(input, defaultToolContext(), target);
	}
	else if(options.stage == "vertex")
	{
		result = compiler.compileGraphicsVertex(input, target);
	}
	else
	{
		err << "unsupported stage: " << options.stage << "\n";
		return 1;
	}

	out << result.text;
	return 0;
}

}  // namespace sw

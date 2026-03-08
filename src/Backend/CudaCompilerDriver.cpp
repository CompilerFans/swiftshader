#include "CudaCompilerDriver.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace backend {
namespace {

std::string shellQuote(const std::string &text)
{
	std::string quoted = "'";
	for(char ch : text)
	{
		if(ch == '\'')
		{
			quoted += "'\\''";
		}
		else
		{
			quoted += ch;
		}
	}
	quoted += "'";
	return quoted;
}

std::string readFile(const std::string &path)
{
	std::ifstream stream(path);
	if(!stream.is_open())
	{
		return {};
	}

	std::ostringstream contents;
	contents << stream.rdbuf();
	return contents.str();
}

std::string createWorkingDirectory()
{
	std::array<char, 64> pattern = {};
	std::snprintf(pattern.data(), pattern.size(), "/tmp/swiftshader-cuda-%d-XXXXXX", static_cast<int>(::getpid()));
	char *directory = ::mkdtemp(pattern.data());
	return directory ? std::string(directory) : std::string();
}

int commandExitCode(int result)
{
	if(result == -1)
	{
		return -1;
	}

	if(WIFEXITED(result))
	{
		return WEXITSTATUS(result);
	}

	return result;
}

}  // namespace

std::string CudaCompilerDriver::nvccExecutable()
{
	if(const char *env = std::getenv("NVCC"))
	{
		if(env[0] != '\0')
		{
			return env;
		}
	}

	return "nvcc";
}

bool CudaCompilerDriver::keepArtifacts()
{
	if(const char *env = std::getenv("SWIFTSHADER_KEEP_CUDA_ARTIFACTS"))
	{
		return env[0] != '\0' && env[0] != '0';
	}

	return false;
}

CudaCompileResult CudaCompilerDriver::compileToFatbin(const std::string &source, const std::string &gpuArchitecture) const
{
	CudaCompileResult result = {};
	result.workingDirectory = createWorkingDirectory();
	if(result.workingDirectory.empty())
	{
		result.errorMessage = "failed to create temporary CUDA working directory";
		return result;
	}

	const std::string sourcePath = result.workingDirectory + "/module.cu";
	result.modulePath = result.workingDirectory + "/module.fatbin";
	result.logPath = result.workingDirectory + "/nvcc.log";

	{
		std::ofstream sourceFile(sourcePath, std::ios::binary);
		sourceFile << source;
	}

	const std::string command =
	    shellQuote(nvccExecutable()) + " --fatbin --std=c++17 -arch=" + gpuArchitecture +
	    " -o " + shellQuote(result.modulePath) + " " + shellQuote(sourcePath) +
	    " > " + shellQuote(result.logPath) + " 2>&1";

	const int compileResult = std::system(command.c_str());
	if(commandExitCode(compileResult) != 0 || !std::filesystem::exists(result.modulePath))
	{
		result.errorMessage = "nvcc compilation failed";
		const std::string logContents = readFile(result.logPath);
		if(!logContents.empty())
		{
			result.errorMessage += ": " + logContents;
		}
		return result;
	}

	result.succeeded = true;
	return result;
}

}  // namespace backend

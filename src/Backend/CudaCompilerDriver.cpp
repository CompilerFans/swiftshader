#include "CudaCompilerDriver.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <spawn.h>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace backend {
namespace {

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

int spawnProcess(const std::string &executable,
                 const std::vector<std::string> &arguments,
                 const std::string &logPath,
                 std::string *errorMessage)
{
	if(errorMessage)
	{
		errorMessage->clear();
	}

	const int logFd = ::open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(logFd < 0)
	{
		if(errorMessage)
		{
			*errorMessage = "failed to open nvcc log file: ";
			*errorMessage += std::strerror(errno);
		}
		return -1;
	}

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, logFd, STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&actions, logFd, STDERR_FILENO);
	posix_spawn_file_actions_addclose(&actions, logFd);

	std::vector<char *> argv;
	argv.reserve(arguments.size() + 2);
	argv.push_back(const_cast<char *>(executable.c_str()));
	for(const auto &arg : arguments)
	{
		argv.push_back(const_cast<char *>(arg.c_str()));
	}
	argv.push_back(nullptr);

	pid_t pid = 0;
	const int spawnResult = ::posix_spawnp(&pid, executable.c_str(), &actions, nullptr, argv.data(), environ);
	posix_spawn_file_actions_destroy(&actions);
	::close(logFd);

	if(spawnResult != 0)
	{
		std::ofstream logFile(logPath, std::ios::app);
		logFile << "posix_spawnp failed: " << std::strerror(spawnResult) << "\n";
		if(errorMessage)
		{
			*errorMessage = "failed to launch nvcc: ";
			*errorMessage += std::strerror(spawnResult);
		}
		return -1;
	}

	int status = 0;
	if(::waitpid(pid, &status, 0) < 0)
	{
		std::ofstream logFile(logPath, std::ios::app);
		logFile << "waitpid failed: " << std::strerror(errno) << "\n";
		if(errorMessage)
		{
			*errorMessage = "failed to wait for nvcc: ";
			*errorMessage += std::strerror(errno);
		}
		return -1;
	}

	return commandExitCode(status);
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

	const std::string nvcc = nvccExecutable();
	std::vector<std::string> args;
	args.reserve(6);
	args.emplace_back("--fatbin");
	args.emplace_back("--std=c++17");
	args.emplace_back("-arch=" + gpuArchitecture);
	args.emplace_back("-o");
	args.emplace_back(result.modulePath);
	args.emplace_back(sourcePath);

	std::string spawnError;
	const int compileResult = spawnProcess(nvcc, args, result.logPath, &spawnError);
	if(compileResult != 0 || !std::filesystem::exists(result.modulePath))
	{
		result.errorMessage = "nvcc compilation failed";
		if(!spawnError.empty())
		{
			result.errorMessage += ": " + spawnError;
		}
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

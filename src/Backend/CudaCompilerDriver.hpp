#ifndef SWIFTSHADER_CUDA_COMPILER_DRIVER_HPP_
#define SWIFTSHADER_CUDA_COMPILER_DRIVER_HPP_

#include <string>

namespace backend {

struct CudaCompileResult
{
	bool succeeded = false;
	std::string modulePath;
	std::string logPath;
	std::string workingDirectory;
	std::string errorMessage;
};

class CudaCompilerDriver
{
public:
	CudaCompileResult compileToFatbin(const std::string &source, const std::string &gpuArchitecture) const;

	static std::string nvccExecutable();
	static bool keepArtifacts();
};

}  // namespace backend

#endif  // SWIFTSHADER_CUDA_COMPILER_DRIVER_HPP_

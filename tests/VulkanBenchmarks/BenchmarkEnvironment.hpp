#ifndef TESTS_VULKANBENCHMARKS_BENCHMARKENVIRONMENT_HPP_
#define TESTS_VULKANBENCHMARKS_BENCHMARKENVIRONMENT_HPP_

#include <cstdlib>

namespace benchmarkutil {

inline void setProcessEnvironmentVariable(const char *name, const char *value)
{
#if defined(_WIN32)
	[[maybe_unused]] auto result = ::_putenv_s(name, value);
#else
	[[maybe_unused]] auto result = ::setenv(name, value, 1);
#endif
}

inline void configureRuntimeEnvironment()
{
	setProcessEnvironmentVariable("SWIFTSHADER_CUDA_DUMP_SOURCE", "0");
	setProcessEnvironmentVariable("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1");
}

}  // namespace benchmarkutil

#endif  // TESTS_VULKANBENCHMARKS_BENCHMARKENVIRONMENT_HPP_

#include "Backend/FakeRuntimeAPI.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#	include <cstdlib>
#endif

#include <gtest/gtest.h>

TEST(RuntimeAPI, FakeRuntimeCreatesModuleHandle)
{
    backend::FakeRuntimeAPI api;
    auto module = api.createModule("kernel text");
    EXPECT_TRUE(module.valid());
}

TEST(RuntimeAPI, FakeRuntimeCreatesModuleHandleWithCustomEntrypoint)
{
	backend::FakeRuntimeAPI api;
	auto module = api.createModule("kernel text", "vs_entry");
	EXPECT_TRUE(module.valid());
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(RuntimeAPI, CudaRuntimePrintsKernelSourceByDefault)
{
	::unsetenv("SWIFTSHADER_CUDA_DUMP_SOURCE");

	backend::CudaRuntimeAPI api;
	ASSERT_TRUE(api.isAvailable()) << api.initializationError();

	const char *source = R"(
extern "C" __global__ void kernel_main(unsigned int *value)
{
	if(blockIdx.x == 0 && threadIdx.x == 0)
	{
		*value = 7u;
	}
}
)";

	testing::internal::CaptureStderr();
	auto module = api.createModule(source);
	std::string dump = testing::internal::GetCapturedStderr();

	ASSERT_TRUE(module.valid()) << api.initializationError();
	EXPECT_NE(dump.find("SWIFTSHADER CUDA SOURCE BEGIN"), std::string::npos);
	EXPECT_NE(dump.find("kernel_main"), std::string::npos);
	EXPECT_NE(dump.find("*value = 7u;"), std::string::npos);
	EXPECT_NE(dump.find("SWIFTSHADER CUDA SOURCE END"), std::string::npos);
}

TEST(RuntimeAPI, CudaRuntimeSuppressesKernelSourceWhenDisabled)
{
	::setenv("SWIFTSHADER_CUDA_DUMP_SOURCE", "off", 1);

	backend::CudaRuntimeAPI api;
	ASSERT_TRUE(api.isAvailable()) << api.initializationError();

	const char *source = R"(
extern "C" __global__ void kernel_main(unsigned int *value)
{
	if(blockIdx.x == 0 && threadIdx.x == 0)
	{
		*value = 9u;
	}
}
)";

	testing::internal::CaptureStderr();
	auto module = api.createModule(source);
	std::string dump = testing::internal::GetCapturedStderr();

	ASSERT_TRUE(module.valid()) << api.initializationError();
	EXPECT_EQ(dump.find("SWIFTSHADER CUDA SOURCE BEGIN"), std::string::npos);
	EXPECT_EQ(dump.find("kernel_main"), std::string::npos);
	EXPECT_EQ(dump.find("SWIFTSHADER CUDA SOURCE END"), std::string::npos);

	::unsetenv("SWIFTSHADER_CUDA_DUMP_SOURCE");
}

TEST(RuntimeAPI, CudaRuntimeCompilesLaunchesAndReadsBackDeviceMemory)
{
	backend::CudaRuntimeAPI api;
	ASSERT_TRUE(api.isAvailable()) << api.initializationError();

	const char *source = R"(
extern "C" __global__ void kernel_main(unsigned int *value)
{
	if(blockIdx.x == 0 && threadIdx.x == 0)
	{
		*value = 0x1234ABCDu;
	}
}
)";

	auto module = api.createModule(source);
	ASSERT_TRUE(module.valid()) << api.initializationError();

	auto memory = api.allocateMemory(sizeof(uint32_t));
	ASSERT_TRUE(memory.valid()) << api.initializationError();

	uint32_t initial = 0;
	api.copyHostToMemory(memory, &initial, sizeof(initial));

	uint64_t devicePointer = api.memoryAddress(memory);
	std::vector<void *> arguments = { &devicePointer };

	backend::LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = 1;
	record.blockCountY = 1;
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();

	api.launch(module, record, arguments);
	api.synchronize();

	uint32_t value = 0;
	api.copyMemoryToHost(&value, memory, sizeof(value));
	EXPECT_EQ(value, 0x1234ABCDu);

	api.freeMemory(memory);
}

TEST(RuntimeAPI, CudaRuntimeLaunchesCustomEntrypoint)
{
	backend::CudaRuntimeAPI api;
	ASSERT_TRUE(api.isAvailable()) << api.initializationError();

	const char *source = R"(
extern "C" __global__ void vs_entry(unsigned int *value)
{
	if(blockIdx.x == 0 && threadIdx.x == 0)
	{
		*value = 0xABCDEF01u;
	}
}
)";

	auto module = api.createModule(source, "vs_entry");
	ASSERT_TRUE(module.valid()) << api.initializationError();

	auto memory = api.allocateMemory(sizeof(uint32_t));
	ASSERT_TRUE(memory.valid()) << api.initializationError();

	uint32_t initial = 0;
	api.copyHostToMemory(memory, &initial, sizeof(initial));

	uint64_t devicePointer = api.memoryAddress(memory);
	std::vector<void *> arguments = { &devicePointer };

	backend::LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = 1;
	record.blockCountY = 1;
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();

	api.launch(module, record, arguments);
	api.synchronize();

	uint32_t value = 0;
	api.copyMemoryToHost(&value, memory, sizeof(value));
	EXPECT_EQ(value, 0xABCDEF01u);

	api.freeMemory(memory);
}
#endif

#include "Backend/FakeRuntimeAPI.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <gtest/gtest.h>

TEST(RuntimeAPI, FakeRuntimeCreatesModuleHandle)
{
    backend::FakeRuntimeAPI api;
    auto module = api.createModule("kernel text");
    EXPECT_TRUE(module.valid());
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
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
#endif

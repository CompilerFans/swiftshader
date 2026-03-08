#include "Vulkan/VkPipeline.hpp"
#include "Device.hpp"
#include "Driver.hpp"

#include "gtest/gtest.h"

#include <spirv/unified1/spirv.hpp>

#include <vector>

namespace {

std::vector<uint32_t> minimalComputeShaderBinary()
{
	const uint32_t words[] = {
		spv::MagicNumber,
		0x00010300,
		0,
		4,
		0,
		(2u << 16) | spv::OpCapability, spv::CapabilityShader,
		(3u << 16) | spv::OpMemoryModel, spv::AddressingModelLogical, spv::MemoryModelGLSL450,
		(6u << 16) | spv::OpEntryPoint, spv::ExecutionModelGLCompute, 1u, 0x6E69616Du,
		(6u << 16) | spv::OpExecutionMode, 1u, spv::ExecutionModeLocalSize, 1u, 1u, 1u,
		(2u << 16) | spv::OpTypeVoid, 2u,
		(3u << 16) | spv::OpTypeFunction, 3u, 2u,
		(5u << 16) | spv::OpFunction, 2u, 1u, 0u, 3u,
		(2u << 16) | spv::OpLabel, 4u,
		(1u << 16) | spv::OpReturn,
		(1u << 16) | spv::OpFunctionEnd,
	};

	return std::vector<uint32_t>(words, words + sizeof(words) / sizeof(words[0]));
}

class ComputeBackendPipelineTest : public testing::Test
{
protected:
	static Driver driver;

	static void SetUpTestSuite()
	{
		ASSERT_TRUE(driver.loadSwiftShader());
	}

	static void TearDownTestSuite()
	{
		driver.unload();
	}
};

Driver ComputeBackendPipelineTest::driver;

}  // namespace

TEST_F(ComputeBackendPipelineTest, BuildBackendExecutableWithoutDispatch)
{
	const VkInstanceCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, nullptr, 0, nullptr, 0, nullptr,
	};

	VkInstance instance = VK_NULL_HANDLE;
	ASSERT_EQ(driver.vkCreateInstance(&createInfo, nullptr, &instance), VK_SUCCESS);
	ASSERT_TRUE(driver.resolve(instance));

	std::unique_ptr<Device> device;
	ASSERT_EQ(Device::CreateComputeDevice(&driver, instance, device), VK_SUCCESS);
	ASSERT_TRUE(device->IsValid());

	auto spirv = minimalComputeShaderBinary();

	VkShaderModule shaderModule = {};
	ASSERT_EQ(device->CreateShaderModule(spirv, &shaderModule), VK_SUCCESS);

	VkDescriptorSetLayout descriptorSetLayout = {};
	ASSERT_EQ(device->CreateDescriptorSetLayout({}, &descriptorSetLayout), VK_SUCCESS);

	VkPipelineLayout pipelineLayout = {};
	ASSERT_EQ(device->CreatePipelineLayout(descriptorSetLayout, &pipelineLayout), VK_SUCCESS);

	VkPipeline pipeline = {};
	ASSERT_EQ(device->CreateComputePipeline(shaderModule, pipelineLayout, &pipeline), VK_SUCCESS);

	auto *computePipeline = reinterpret_cast<vk::ComputePipeline *>(static_cast<void *>(pipeline));
	ASSERT_NE(computePipeline, nullptr);
	EXPECT_TRUE(computePipeline->hasBackendExecutable());

	device->DestroyPipeline(pipeline);
	device->DestroyPipelineLayout(pipelineLayout);
	device->DestroyDescriptorSetLayout(descriptorSetLayout);
	device->DestroyShaderModule(shaderModule);
	driver.vkDestroyInstance(instance, nullptr);
}

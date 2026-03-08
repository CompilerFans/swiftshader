#include "FakeRuntimeAPI.hpp"

#include <cassert>
#include <cstring>

namespace backend {
namespace {

std::string gLastModuleSource;
LaunchRecord gLastLaunch = {};
uint32_t gLaunchCount = 0;

}  // namespace

void FakeRuntimeAPI::resetGlobalCapture()
{
	gLastModuleSource.clear();
	gLastLaunch = {};
	gLaunchCount = 0;
}

const std::string &FakeRuntimeAPI::globalLastModuleSource()
{
	return gLastModuleSource;
}

const LaunchRecord &FakeRuntimeAPI::globalLastLaunch()
{
	return gLastLaunch;
}

uint32_t FakeRuntimeAPI::globalLaunchCount()
{
	return gLaunchCount;
}

ModuleHandle FakeRuntimeAPI::createModule(const std::string &sourceOrIR)
{
	moduleSource = sourceOrIR;
	gLastModuleSource = sourceOrIR;
	return ModuleHandle{ nextId++ };
}

DeviceMemoryHandle FakeRuntimeAPI::allocateMemory(size_t numBytes)
{
	auto handle = DeviceMemoryHandle{ nextMemoryId++ };
	allocations.emplace(handle.id, std::vector<uint8_t>(numBytes, 0));
	return handle;
}

void FakeRuntimeAPI::freeMemory(DeviceMemoryHandle memory)
{
	allocations.erase(memory.id);
}

void FakeRuntimeAPI::copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes)
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	assert(it->second.size() >= numBytes);
	std::memcpy(it->second.data(), source, numBytes);
}

void FakeRuntimeAPI::copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes)
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	assert(it->second.size() >= numBytes);
	std::memcpy(destination, it->second.data(), numBytes);
}

uint64_t FakeRuntimeAPI::memoryAddress(DeviceMemoryHandle memory) const
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(it->second.data()));
}

void FakeRuntimeAPI::launch(ModuleHandle module, const LaunchRecord &record, const std::vector<void *> &arguments)
{
	launchRecord = record;
	launchRecord.module = module;
	launchRecord.argumentCount = arguments.size();
	gLastLaunch = launchRecord;
	gLaunchCount++;
}

}  // namespace backend

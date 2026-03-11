#include "StubRuntimeAPI.hpp"

#include <cassert>
#include <cstring>

namespace backend {
namespace {

std::string gLastModuleSource;
LaunchRecord gLastLaunch = {};
uint32_t gLaunchCount = 0;

}  // namespace

void StubRuntimeAPI::resetGlobalCapture()
{
	gLastModuleSource.clear();
	gLastLaunch = {};
	gLaunchCount = 0;
}

const std::string &StubRuntimeAPI::globalLastModuleSource()
{
	return gLastModuleSource;
}

const LaunchRecord &StubRuntimeAPI::globalLastLaunch()
{
	return gLastLaunch;
}

uint32_t StubRuntimeAPI::globalLaunchCount()
{
	return gLaunchCount;
}

ModuleHandle StubRuntimeAPI::createModule(const std::string &sourceOrIR, const std::string &entryPoint)
{
	moduleSource = sourceOrIR;
	gLastModuleSource = sourceOrIR;
	auto handle = ModuleHandle{ nextId++ };
	moduleEntrypoints.emplace(handle.id, entryPoint);
	return handle;
}

DeviceMemoryHandle StubRuntimeAPI::allocateMemory(size_t numBytes)
{
	auto handle = DeviceMemoryHandle{ nextMemoryId++ };
	allocations.emplace(handle.id, std::vector<uint8_t>(numBytes, 0));
	return handle;
}

void StubRuntimeAPI::freeMemory(DeviceMemoryHandle memory)
{
	allocations.erase(memory.id);
}

void StubRuntimeAPI::copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes)
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	assert(it->second.size() >= numBytes);
	std::memcpy(it->second.data(), source, numBytes);
}

void StubRuntimeAPI::copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes)
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	assert(it->second.size() >= numBytes);
	std::memcpy(destination, it->second.data(), numBytes);
}

uint64_t StubRuntimeAPI::memoryAddress(DeviceMemoryHandle memory) const
{
	auto it = allocations.find(memory.id);
	assert(it != allocations.end());
	return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(it->second.data()));
}

void StubRuntimeAPI::launch(ModuleHandle module, const LaunchRecord &record, const std::vector<void *> &arguments)
{
	launchRecord = record;
	launchRecord.module = module;
	launchRecord.argumentCount = arguments.size();
	gLastLaunch = launchRecord;
	gLaunchCount++;
}

}  // namespace backend

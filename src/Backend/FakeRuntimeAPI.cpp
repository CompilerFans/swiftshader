#include "FakeRuntimeAPI.hpp"

namespace backend {
namespace {

std::string gLastModuleSource;
LaunchRecord gLastLaunch = {};

}  // namespace

void FakeRuntimeAPI::resetGlobalCapture()
{
	gLastModuleSource.clear();
	gLastLaunch = {};
}

const std::string &FakeRuntimeAPI::globalLastModuleSource()
{
	return gLastModuleSource;
}

const LaunchRecord &FakeRuntimeAPI::globalLastLaunch()
{
	return gLastLaunch;
}

ModuleHandle FakeRuntimeAPI::createModule(const std::string &sourceOrIR)
{
	moduleSource = sourceOrIR;
	gLastModuleSource = sourceOrIR;
	return ModuleHandle{ nextId++ };
}

void FakeRuntimeAPI::launch(ModuleHandle module, const LaunchRecord &record)
{
	launchRecord = record;
	launchRecord.module = module;
	gLastLaunch = launchRecord;
}

}  // namespace backend

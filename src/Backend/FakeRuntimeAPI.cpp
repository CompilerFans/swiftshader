#include "FakeRuntimeAPI.hpp"

namespace backend {

ModuleHandle FakeRuntimeAPI::createModule(const std::string &sourceOrIR)
{
	moduleSource = sourceOrIR;
	return ModuleHandle{ nextId++ };
}

void FakeRuntimeAPI::launch(ModuleHandle module, const LaunchRecord &record)
{
	launchRecord = record;
	launchRecord.module = module;
}

}  // namespace backend

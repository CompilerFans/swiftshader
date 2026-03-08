#ifndef SWIFTSHADER_FAKE_RUNTIME_API_HPP_
#define SWIFTSHADER_FAKE_RUNTIME_API_HPP_

#include "RuntimeAPI.hpp"

namespace backend {

class FakeRuntimeAPI : public RuntimeAPI
{
public:
	static void resetGlobalCapture();
	static const std::string &globalLastModuleSource();
	static const LaunchRecord &globalLastLaunch();

	ModuleHandle createModule(const std::string &sourceOrIR) override;
	void launch(ModuleHandle module, const LaunchRecord &record) override;

	const std::string &lastModuleSource() const { return moduleSource; }
	const LaunchRecord &lastLaunch() const { return launchRecord; }

private:
	uint64_t nextId = 1;
	std::string moduleSource;
	LaunchRecord launchRecord = {};
};

}  // namespace backend

#endif  // SWIFTSHADER_FAKE_RUNTIME_API_HPP_

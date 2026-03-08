#ifndef SWIFTSHADER_BACKEND_FACTORY_HPP_
#define SWIFTSHADER_BACKEND_FACTORY_HPP_

#include "ExecutionBackend.hpp"
#include "RuntimeAPI.hpp"

namespace vk { class Device; }

namespace backend {

BackendKind defaultBackendKind();
std::unique_ptr<ExecutionBackend> createExecutionBackend(vk::Device *device);
std::unique_ptr<RuntimeAPI> createRuntimeAPI();

}  // namespace backend

#endif  // SWIFTSHADER_BACKEND_FACTORY_HPP_

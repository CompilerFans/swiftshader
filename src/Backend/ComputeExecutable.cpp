#include "ComputeExecutable.hpp"

#include "Pipeline/CudaLikeSourceEmitter.hpp"
#include "Pipeline/KernelIR.hpp"
#include "Pipeline/SemanticIRBuilder.hpp"

namespace backend {

std::shared_ptr<ComputeExecutable> ComputeExecutable::create(const sw::ParsedSpirvInfo &parsed)
{
	if(parsed.stage != VK_SHADER_STAGE_COMPUTE_BIT)
	{
		return nullptr;
	}

	sw::KernelIRModule kernel;
	auto source = sw::emitCudaLikeSource(kernel);
	return std::shared_ptr<ComputeExecutable>(new ComputeExecutable(source));
}

ModuleHandle ComputeExecutable::ensureModule(RuntimeAPI &runtime)
{
	if(!module.valid())
	{
		module = runtime.createModule(source);
	}
	return module;
}

void ComputeExecutable::dispatch(RuntimeAPI &runtime, const ComputeDispatchInfo &dispatchInfo)
{
	LaunchRecord record = {};
	record.baseGroupX = dispatchInfo.baseGroupX;
	record.baseGroupY = dispatchInfo.baseGroupY;
	record.baseGroupZ = dispatchInfo.baseGroupZ;
	record.groupCountX = dispatchInfo.groupCountX;
	record.groupCountY = dispatchInfo.groupCountY;
	record.groupCountZ = dispatchInfo.groupCountZ;
	record.bindingCount = dispatchInfo.bindingCount;
	record.argumentWords = dispatchInfo.argumentWords;
	runtime.launch(ensureModule(runtime), record, {});
}

}  // namespace backend

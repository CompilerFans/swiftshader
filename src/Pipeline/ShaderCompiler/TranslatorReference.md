# Shader Compiler Translator Reference

## Purpose

This note records the most relevant upstream translation paths for the long-term
`SPIR-V bc/txt -> IR -> LLVM IR / target backend` direction of the standalone
`ShaderCompiler` module.

It is not a commitment to vendor any one translator wholesale. The intent is to
capture the parts of those designs that should shape the SwiftShader-side
compiler architecture.

## Current SwiftShader-side baseline

The current standalone module in [`src/Pipeline/ShaderCompiler/`](.) is still a
bring-up compiler:

- It accepts `ShaderModuleInput` from SPIR-V assembly text or binary.
- It performs narrow shader analysis in [`ShaderCompilerAnalysis.cpp`](./ShaderCompilerAnalysis.cpp).
- It lowers minimal vertex information through `SemanticIRBuilder -> KernelIR`.
- It emits either CUDA-like source or LLVM IR skeletons.

That is enough for targeted tests and offline/API smoke usage, but it is not yet
a real standards-level SPIR-V translator.

## LLPC translator: what is worth copying

The most useful local reference is `third_party/llpc/llpc/translator/`.

### 1. Public API is already shaped around entry-point translation

`third_party/llpc/llpc/translator/include/LLVMSPIRVLib.h` exposes:

- `readSpirv(...)`
- explicit `spv::ExecutionModel`
- explicit entry-point name
- specialization constant map
- converting sampler table
- output `llvm::Module`

That is the right shape for a real compiler front-end: translation is entry
point + stage specific, not “whole module with no execution context”.

### 2. Translation is stage-aware from the first step

`SPIRVToLLVM::translate(...)` in
`third_party/llpc/llpc/translator/lib/SPIRV/SPIRVReader.cpp` does the following
very early:

- resolves the targeted entry point from `ExecutionModel + entry name`
- captures execution modes from the selected entry
- sets shader-stage specific pipeline modes
- translates only the targeted entry point plus non-entry helper functions

That matches what the standalone `ShaderCompiler` should eventually do:

- `ShaderModuleInput` must preserve stage + entry point as first-class inputs
- module analysis must be entry-point scoped
- execution-mode metadata must survive into the next IR layer

### 3. Decorations are translated into structured metadata, not ad-hoc queries

`SPIRVToLLVM::transDecoration(...)` is the key reference point for shader IO and
resource semantics.

It explicitly handles:

- `Location`
- `BuiltIn`
- interpolation decorations such as `Flat`, `NoPerspective`, `Centroid`, `Sample`
- `Patch` / `PerPrimitive`
- transform-feedback related decorations
- `Binding`
- `DescriptorSet`

And it maps them into structured LLVM metadata for:

- shader inputs/outputs
- resource blocks
- descriptor-backed globals

That is the strongest architectural lesson for SwiftShader:

- real shader translation cannot stop at “this looks like a constant color
  fragment template”
- the compiler needs a stable metadata model for builtins, resource bindings,
  interpolation mode, and execution semantics

Our current `ShaderCompilerAnalysisResult` and `KernelIR` are only the first
thin slice of that idea.

### 4. Image operations are lowered through a reusable descriptor-extraction layer

Image sampling in LLPC is not lowered by matching one opcode at a time in
isolation. The flow is:

1. `getImageDesc(...)`
2. `setupImageAddressOperands(...)`
3. `transSPIRVImageSampleFromInst(...)`
4. builder call such as `CreateImageSample(...)`

Important details:

- sampled/separate image cases are normalized into an `ExtractedImageInfo`
  structure
- descriptor pointers, plane stride, sampler pointer, compare parameter pointer
  and non-uniform flags are all extracted before emission
- image sampling lowering is stage/resource aware, not a textual template

That is directly relevant to the SwiftShader compiler roadmap:

- current fragment template recognition is useful for bootstrap only
- real support for standard SPIR-V image ops needs an IR node family for:
  - image descriptor access
  - sampler descriptor access
  - image operand flags
  - projected sample / dref / explicit lod / gradients / offsets
  - non-uniform descriptor semantics

### 5. Execution modes and shader modes are module metadata, not side channels

In LLPC, `translate(...)` and `transMetadata()` derive stage-specific modes such
as:

- vertex / tessellation / geometry / fragment / compute stage identity
- fragment `RequireFullQuadsKHR`
- tessellation spacing/order/output vertices
- geometry input/output primitive shape
- workgroup sizes
- floating-point denorm / rounding / fast-math mode

For SwiftShader, that implies the future compiler module should explicitly carry
at least:

- stage
- entry point
- execution modes
- builtin set
- descriptor/resource set
- per-stage fixed-function relevant metadata

Those should be represented in the standalone compiler IR, not inferred again in
every backend-specific emitter.

## MLIR SPIR-V -> LLVM dialect: what it gives us, and what it does not

The local reference is:

- `third_party/llvm-project/mlir/docs/SPIRVToLLVMDialectConversion.md`

### Useful properties

The MLIR route already defines a clean staged lowering:

- SPIR-V dialect
- LLVM dialect
- then standard LLVM IR / backend pipeline

The document explicitly shows:

- scalar/vector/pointer/array/struct type mappings
- a conversion pass entry point:
  - `mlir-opt -convert-spirv-to-llvm`
- direct lowering patterns for many arithmetic and bitwise operations

This is a strong fit for the long-term target architecture:

- keep parsing/semantic reconstruction separate from target code generation
- prefer structured IR-to-IR conversion over direct textual code emission

### Current hard limits

The same MLIR document also calls out important gaps:

- pointer storage class is not fully preserved in LLVM lowering
- struct member decorations are not converted
- image types are not supported
- matrix types are not supported
- many operations remain partial or unsupported

That means the MLIR path is not yet a drop-in replacement for our current
graphics shader needs, especially for:

- descriptor-backed resources
- image/sampler operations
- decorated shader interfaces
- graphics-stage builtin-heavy lowering

So the right conclusion is:

- MLIR is the correct long-term backbone
- but it is not sufficient by itself for the near-term SwiftShader graphics
  bring-up surface

## Recommended SwiftShader architecture from these references

### Stage 1: keep a standalone normalized compiler IR

Before choosing CUDA source, LLVM IR, MLIR SPIR-V dialect, or another backend,
the standalone compiler should own a target-independent IR layer that captures:

- stage + entry point
- execution modes
- builtin usage
- descriptor/resource metadata
- interface variables and interpolation mode
- image/sampler operations
- control-flow / memory / atomics capability masks

That is the missing middle layer between:

- current narrow `ShaderCompilerAnalysis`
- and a real SPIR-V translator

### Stage 2: branch lowering after semantics are reconstructed

Once that normalized IR exists, we can have multiple backends:

1. Bootstrap path
   - current CUDA-like source emitter
   - current LLVM IR skeleton emitter

2. Serious compiler path
   - lower to MLIR SPIR-V / custom dialect mix where feasible
   - convert to LLVM dialect / LLVM IR
   - feed NVPTX or a private backend

3. Compatibility/reference path
   - compare against LLPC/SPIRV-LLVM-Translator semantics for selected features

### Stage 3: keep offline CLI and online API on the same module

The current `shader-compiler` CLI and in-process API already share the same
standalone module. That is the correct direction and should be preserved.

The real compiler should not have separate semantic paths for:

- offline `spv` / `spvasm` compilation
- Vulkan pipeline-time translation

Instead:

- both should use the same parser + semantic IR builder
- Vulkan-specific state should be threaded in as explicit context, similar in
  spirit to LLPC’s shader options / pipeline context inputs

## Concrete implementation guidance for the next SwiftShader steps

### Near-term

- Keep extending `ShaderCompiler` as the standalone module boundary.
- Continue moving shader semantics out of `GraphicsExecutable` ad-hoc analysis.
- Add explicit IR nodes/metadata for:
  - builtins
  - shader IO
  - descriptor bindings
  - image/sample operations

### Mid-term

- Split current “analysis” into:
  - parser / loader
  - semantic reconstruction
  - resource/interface metadata
  - target lowering
- Add binary/text parity tests for the same semantic cases.
- Stop growing direct source-template emission as the primary semantics carrier.

### Long-term

- Use MLIR SPIR-V / LLVM dialect conversion where it is genuinely helpful.
- Fill unsupported image/decoration gaps with either:
  - a SwiftShader-side normalized IR before MLIR lowering, or
  - a custom dialect/lowering layer adjacent to MLIR
- Treat LLPC translator behavior as a semantic reference, not as a dependency to
  embed directly; LLPC’s tight coupling to `lgc::Builder`, pipeline context, and
  AMD-specific middle-end would be too heavy for the current SwiftShader module.

## Bottom line

The local evidence points to one consistent conclusion:

- `ShaderCompiler` should become the owner of a real stage-aware SPIR-V semantic
  front-end
- its next architectural milestone is not “more emitted CUDA text templates”
  but “a richer normalized IR carrying entry-point, builtin, descriptor, and
  image semantics”
- MLIR SPIR-V -> LLVM is the right downstream direction, but only after that
  semantic layer exists
- LLPC translator is most valuable as a reference for how to reconstruct and
  preserve shader semantics before backend lowering

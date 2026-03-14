#include "Pipeline/ShaderCompiler/ShaderCompilerTool.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include "spirv-tools/libspirv.hpp"
#include <vector>

namespace {

std::vector<uint32_t> compileSpirv(const char *assembly, spv_target_env env = SPV_ENV_VULKAN_1_0)
{
    spvtools::SpirvTools core(env);
    core.SetMessageConsumer([](spv_message_level_t, const char *, const spv_position_t &position, const char *message) {
        FAIL() << position.line << ":" << position.column << ": " << message;
    });

    std::vector<uint32_t> spirv;
    EXPECT_TRUE(core.Assemble(assembly, &spirv));
    EXPECT_TRUE(core.Validate(spirv));
    return spirv;
}

constexpr const char kCombinedImageSamplerAssembly[] =
    "OpCapability Shader\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %inTexCoord %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpDecorate %inTexCoord Location 0\n"
    "OpDecorate %outColor Location 0\n"
    "OpDecorate %tex DescriptorSet 0\n"
    "OpDecorate %tex Binding 1\n"
    "%void = OpTypeVoid\n"
    "%func = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v2float = OpTypeVector %float 2\n"
    "%v4float = OpTypeVector %float 4\n"
    "%image = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
    "%sampledImage = OpTypeSampledImage %image\n"
    "%ptrInputV2 = OpTypePointer Input %v2float\n"
    "%ptrOutputV4 = OpTypePointer Output %v4float\n"
    "%ptrUniformConstantSampledImage = OpTypePointer UniformConstant %sampledImage\n"
    "%inTexCoord = OpVariable %ptrInputV2 Input\n"
    "%outColor = OpVariable %ptrOutputV4 Output\n"
    "%tex = OpVariable %ptrUniformConstantSampledImage UniformConstant\n"
    "%main = OpFunction %void None %func\n"
    "%entry = OpLabel\n"
    "%coord = OpLoad %v2float %inTexCoord\n"
    "%sampler = OpLoad %sampledImage %tex\n"
    "%color = OpImageSampleImplicitLod %v4float %sampler %coord\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kVertexPositionAssembly[] =
    "OpCapability Shader\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Vertex %main \"main\" %inPos %gl_VertexIndex %gl_InstanceIndex %gl_PerVertex\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %inPos \"inPos\"\n"
    "OpName %gl_VertexIndex \"gl_VertexIndex\"\n"
    "OpName %gl_InstanceIndex \"gl_InstanceIndex\"\n"
    "OpName %gl_PerVertex \"gl_PerVertex\"\n"
    "OpMemberName %gl_PerVertex_t 0 \"gl_Position\"\n"
    "OpMemberName %gl_PerVertex_t 1 \"gl_PointSize\"\n"
    "OpMemberName %gl_PerVertex_t 2 \"gl_ClipDistance\"\n"
    "OpMemberName %gl_PerVertex_t 3 \"gl_CullDistance\"\n"
    "OpDecorate %inPos Location 0\n"
    "OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n"
    "OpDecorate %gl_InstanceIndex BuiltIn InstanceIndex\n"
    "OpMemberDecorate %gl_PerVertex_t 0 BuiltIn Position\n"
    "OpMemberDecorate %gl_PerVertex_t 1 BuiltIn PointSize\n"
    "OpMemberDecorate %gl_PerVertex_t 2 BuiltIn ClipDistance\n"
    "OpMemberDecorate %gl_PerVertex_t 3 BuiltIn CullDistance\n"
    "OpDecorate %gl_PerVertex_t Block\n"
    "%void = OpTypeVoid\n"
    "%func = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v3float = OpTypeVector %float 3\n"
    "%v4float = OpTypeVector %float 4\n"
    "%int = OpTypeInt 32 1\n"
    "%uint = OpTypeInt 32 0\n"
    "%uint_1 = OpConstant %uint 1\n"
    "%int_0 = OpConstant %int 0\n"
    "%float_0 = OpConstant %float 0\n"
    "%float_1 = OpConstant %float 1\n"
    "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
    "%gl_PerVertex_t = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
    "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
    "%_ptr_Input_int = OpTypePointer Input %int\n"
    "%_ptr_Output_gl_PerVertex_t = OpTypePointer Output %gl_PerVertex_t\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%inPos = OpVariable %_ptr_Input_v3float Input\n"
    "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n"
    "%gl_InstanceIndex = OpVariable %_ptr_Input_int Input\n"
    "%gl_PerVertex = OpVariable %_ptr_Output_gl_PerVertex_t Output\n"
    "%pos = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1\n"
    "%main = OpFunction %void None %func\n"
    "%entry = OpLabel\n"
    "%pos_ptr = OpAccessChain %_ptr_Output_v4float %gl_PerVertex %int_0\n"
    "OpStore %pos_ptr %pos\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

class TempFile
{
public:
    explicit TempFile(const char *contents)
    {
        char pathTemplate[] = "/tmp/swiftshader-shader-tool-XXXXXX.spvasm";
        int fd = mkstemps(pathTemplate, 7);
        if(fd == -1)
        {
            ADD_FAILURE() << "mkstemps failed";
            return;
        }
        path = pathTemplate;
        std::ofstream stream(path);
        stream << contents;
        stream.close();
        ::close(fd);
    }

    ~TempFile()
    {
        if(!path.empty())
        {
            std::remove(path.c_str());
        }
    }

    std::string path;
};

class TempBinaryFile
{
public:
    explicit TempBinaryFile(const char *assembly)
    {
        char pathTemplate[] = "/tmp/swiftshader-shader-tool-XXXXXX.spv";
        int fd = mkstemps(pathTemplate, 4);
        if(fd == -1)
        {
            ADD_FAILURE() << "mkstemps failed";
            return;
        }
        path = pathTemplate;
        const auto spirv = compileSpirv(assembly);
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char *>(spirv.data()), static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
        stream.close();
        ::close(fd);
    }

    ~TempBinaryFile()
    {
        if(!path.empty())
        {
            std::remove(path.c_str());
        }
    }

    std::string path;
};

}  // namespace

TEST(ShaderCompilerTool, CompilesAssemblyToCudaLikeSource)
{
    TempFile input(kCombinedImageSamplerAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "fragment",
        "--entry-point", "main",
        "--input-format", "spvasm",
        "--output-format", "cuda",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("extern \"C\" __global__ void fs_entry"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(ShaderCompilerTool, CompilesAssemblyToLlvmIR)
{
    TempFile input(kCombinedImageSamplerAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "fragment",
        "--entry-point", "main",
        "--input-format", "spvasm",
        "--output-format", "llvm",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("define void @fs_entry"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(ShaderCompilerTool, CompilesBinaryToCudaLikeSource)
{
    TempBinaryFile input(kCombinedImageSamplerAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "fragment",
        "--entry-point", "main",
        "--input-format", "spvbin",
        "--output-format", "cuda",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("extern \"C\" __global__ void fs_entry"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(ShaderCompilerTool, CompilesBinaryToLlvmIR)
{
    TempBinaryFile input(kCombinedImageSamplerAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "fragment",
        "--entry-point", "main",
        "--input-format", "spvbin",
        "--output-format", "llvm",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("define void @fs_entry"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(ShaderCompilerTool, CompilesVertexAssemblyToCudaLikeSource)
{
    TempFile input(kVertexPositionAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "vertex",
        "--entry-point", "main",
        "--input-format", "spvasm",
        "--output-format", "cuda",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("extern \"C\" __global__ void vs_entry"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(ShaderCompilerTool, CompilesVertexAssemblyToLlvmIR)
{
    TempFile input(kVertexPositionAssembly);
    std::ostringstream out;
    std::ostringstream err;

    const std::vector<std::string> args = {
        "--stage", "vertex",
        "--entry-point", "main",
        "--input-format", "spvasm",
        "--output-format", "llvm",
        "--input", input.path,
    };

    EXPECT_EQ(sw::runShaderCompilerTool(args, out, err), 0);
    EXPECT_NE(out.str().find("define void @vs_entry(%struct.VsParams* %params)"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

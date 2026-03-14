#include "Pipeline/ShaderCompiler/ShaderCompilerTool.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

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

#ifndef SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_TOOL_HPP_
#define SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_TOOL_HPP_

#include <iosfwd>
#include <string>
#include <vector>

namespace sw {

int runShaderCompilerTool(const std::vector<std::string> &args, std::ostream &out, std::ostream &err);

}  // namespace sw

#endif  // SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_TOOL_HPP_

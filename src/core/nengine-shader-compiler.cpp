
#include <iostream>
#include <sstream>
#include <string>
#include "include/nengine-shader-compiler.h"
#include "shaderc/shaderc.hpp"
#include "include/nengine.h"

std::string shader_compiler::name = "ShaderCompiler";

shader_compiler::shader_compiler()
{
}

shader_compiler::~shader_compiler()
{
}

spirv_module shader_compiler::compile(shader_compile_config& config)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(config.shader_code, config.shader_kind, config.input_file_name.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::ostringstream error_stream;
        error_stream    << nengine::name << " - " << shader_compiler::name 
                        << "Failed to compile shader: " << result.GetErrorMessage() << std::endl;
        throw(error_stream.str());
    }

    spirv_module module(result.cbegin(), result.cend());
    return module;

}

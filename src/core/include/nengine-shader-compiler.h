#pragma once

#include "../../utils/helpers.h"
#include <string>
#include <vector>
#include "shaderc/shaderc.hpp"

typedef  std::vector<uint32_t> spirv_module;

struct shader_compile_config
{
    std::string shader_code;
    shaderc_shader_kind shader_kind;
    std::string entry_point = "main";
    std::string input_file_name = "unnamed_shader";
};

class shader_compiler
{
private:
    shader_compiler();
    ~shader_compiler();

public:
    static std::string name;
    static spirv_module compile(shader_compile_config& config);
};

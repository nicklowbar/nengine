#pragma once

#define UNUSED(x) (void)(x)

namespace nengine_utils
{
    struct version 
    {
        unsigned int variant;
        unsigned int major;
        unsigned int minor;
        unsigned int patch;
    };
}

#include "include/nengine.h"
#include "../utils/helpers.h"


#include <iostream>
#include <memory>

nengine_utils::version nengine::nengine_version = {0, 0, 1, 0};

nengine::nengine(){}

nengine::nengine(nengine_config in_config) : config(in_config) {}

void nengine::initialize()
{
    std::cout << "NeNgine - Main()" << std::endl;
    status = nengine_status::RUNNING;
}

void nengine::tick()
{
    std::cout << "NeNgine - Main()" << std::endl;
}

void nengine::shutdown()
{
    status = nengine_status::STOPPED;
}

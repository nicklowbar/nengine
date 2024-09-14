#include "include/nengine.h"


#include <iostream>
#include <memory>

nengine::nengine(){}

nengine::nengine(nengine_config in_config) : config(in_config) {}

void nengine::main()
{
    std::cout << "NeNgine - Main()" << std::endl;
}

void nengine::tick()
{
    std::cout << "NeNgine - Main()" << std::endl;
}

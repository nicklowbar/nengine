#include "src/core/include/nengine.h"
#include "src/utils/helpers.h"

#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    auto engine_instance = std::make_unique<nengine>();

    std::cout << "Hello World!" << std::endl;
    return EXIT_SUCCESS;
}

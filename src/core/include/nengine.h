#pragma once
#include "../../utils/helpers.h"

#include <glm/glm.hpp>
#include <string>

struct nengine_config
{
    int resolution[2] = {800, 600};
    void * out_texture = nullptr;
};

struct scene
{

};

enum nengine_status
{
    STOPPED = 0,
    RUNNING = 1,
    HALTED = 2,
    ERROR = -1
};


class nengine
{
public:
    nengine();
    nengine(nengine_config in_config);
    static nengine_utils::version nengine_version;
    static std::string name;

public:
    void initialize();
    inline int get_status(){ return status; }
    void shutdown();

protected:    
    void tick();

private:
    nengine_config config;
    nengine_status status = nengine_status::STOPPED;
};

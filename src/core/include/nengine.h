#pragma once

struct nengine_config
{
    int resolution[2] = {800, 600};
    void * out_texture = nullptr;
};

struct scene
{

};

class nengine
{
public:
    nengine();
    nengine(nengine_config in_config);

protected:
    void main();
    void tick();

private:
    nengine_config config;
};

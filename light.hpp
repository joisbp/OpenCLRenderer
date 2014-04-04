#ifndef INCLUDED_LIGHT_HPP
#define INCLUDED_LIGHT_HPP

#include <cl/cl.h>
#include <vector>

struct light
{
    cl_float4 pos;
    cl_float4 col;
    cl_uint shadow;
    cl_float brightness;
    cl_float radius;
    //cl_float2 pad;

    light();

    void set_pos(cl_float4);
    void set_col(cl_float4);
    void set_shadow_bright(cl_uint shadow, cl_float bright);
    void set_type(cl_float);
    void set_radius(cl_float);

    static std::vector<light*> lightlist;

    static int get_light_id(light*);
    static light* add_light(light* l);
    static void remove_light(light* l);
};


#endif

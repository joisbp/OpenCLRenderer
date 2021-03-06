#include "light.hpp"
#include <iostream>
#include <float.h>
#include <algorithm>
#include "clstate.h"
#include "engine.hpp"

std::vector<light*> light::lightlist;
std::vector<cl_uint> light::active;

bool light::dirty_shadow = true;
int light::expected_clear_kernel_size = 256;
bool light::static_lights_are_dirty = true;
bool light::dirty_gpu = false;

void light::set_pos(cl_float4 p)
{
    pos.x=p.x;
    pos.y=p.y;
    pos.z=p.z;
}

void light::set_type(cl_float t)
{
    pos.w = t;
}

void light::set_col(cl_float4 c)
{
    col=c;
}

void light::set_shadow_casting(cl_uint isshadowcasting)
{
    shadow=isshadowcasting;
}

void light::set_brightness(cl_float bright)
{
    brightness=bright;
}

void light::set_radius(cl_float r)
{
    radius = r;
}

void light::set_diffuse(cl_float d)
{
    diffuse = d;
}

void light::set_godray_intensity(cl_float g)
{
    godray_intensity = g;
}

void light::set_is_static(bool st)
{
    if(is_static != st)
        static_lights_are_dirty = true;

    is_static = st;
}

void light::invalidate_buffers()
{
    dirty_shadow = true;

    static_lights_are_dirty = true;

    dirty_gpu = true;
}

void light::set_active(bool s)
{
    int id = get_light_id(this);

    if(id == -1)
        return;

    light::active[id] = s;
}

light::light()
{
    shadow = 0;
    col = {1.f, 1.f, 1.f, 0.f};
    pos = {0.f, 0.f, 0.f, 0.f};
    radius = FLT_MAX/100000.0f;
    brightness = 1.f;
    diffuse = 1.f;
    godray_intensity = 0;
    is_static = 0;
}

int light::get_light_id(light* l)
{
    for(int i=0; i<lightlist.size(); i++)
    {
        if(lightlist[i] == l)
            return i;
    }

    return -1;
}

light* light::add_light(const light* l)
{
    if(l->shadow)
        dirty_shadow = true;

    if(l->is_static)
        static_lights_are_dirty = true;

    light* new_light = new light(*l);
    lightlist.push_back(new_light);
    active.push_back(true);
    return new_light;
}

void light::remove_light(light* l)
{
    int lid = get_light_id(l);

    if(lid == -1)
    {
        lg::log("warning, could not remove light, not found");
        return;
    }

    if(l->is_static)
        static_lights_are_dirty = true;

    if(l->shadow)
        dirty_shadow = true;

    lightlist.erase(lightlist.begin() + lid);
    active.erase(active.begin() + lid);

    delete l;
}

///the writes here need ordering!
light_gpu light::build(light_gpu* old_dat) ///for the moment, just reallocate everything
{
    cl_uint lnum = light::lightlist.size();

    static cl_uint found_num = 0;

    ///turn pointer list into block of memory for writing to gpu
    static std::vector<light> light_straight;

    light_straight.clear();
    found_num = 0;

    bool any_godray = false;

    for(int i=0; i<light::lightlist.size(); i++)
    {
        if(light::active[i] == false)
        {
            continue;
        }

        found_num++;

        if(light::lightlist[i]->godray_intensity > 0)
            any_godray = true;

        light_straight.push_back(*light::lightlist[i]);
    }

    light_gpu dat;

    int clamped_num = std::max((int)found_num, 1);

    ///gpu light memory
    dat.g_light_mem = compute::buffer(cl::context, sizeof(light)*clamped_num, CL_MEM_READ_ONLY);

    ///wtf? Isn't this super dangerous? What if the vector data goes out of scope?
    if(found_num > 0)
    {
        if(!dirty_gpu)
            cl::cqueue.enqueue_write_buffer_async(dat.g_light_mem, 0, sizeof(light)*found_num, light_straight.data());
        else
            cl::cqueue.enqueue_write_buffer(dat.g_light_mem, 0, sizeof(light)*found_num, light_straight.data());

    }

    dat.g_light_num = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_ONLY, nullptr);

    if(!dirty_gpu)
        cl::cqueue.enqueue_write_buffer_async(dat.g_light_num, 0, sizeof(cl_uint), &found_num);
    else
        cl::cqueue.enqueue_write_buffer(dat.g_light_num, 0, sizeof(cl_uint), &found_num);

    dat.any_godray = any_godray;

    ///sacrifice soul to chaos gods, allocate light buffers here

    int shadowcasting_lights_num = 0;

    for(unsigned int i=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i]->shadow == 1)
        {
            shadowcasting_lights_num++;
        }
    }

    ///make this a variable in the opencl code
    const cl_uint l_size = engine::l_size;

    if(dirty_shadow)
    {
        uint32_t total_len = sizeof(cl_uint)*l_size*l_size*6*shadowcasting_lights_num;

        if(total_len % expected_clear_kernel_size != 0)
        {
            int rem = total_len % expected_clear_kernel_size;

            total_len -= rem;
            total_len += expected_clear_kernel_size;
        }

        total_len = std::max(total_len, (uint32_t)sizeof(cl_uint));

        ///blank cubemap filled with UINT_MAX
        engine::g_shadow_light_buffer = compute::buffer(cl::context, total_len, CL_MEM_READ_WRITE, NULL);

        int static_shadow_extra = get_num_static_shadowcasters();

        uint32_t static_len = sizeof(cl_uint) * l_size * l_size * 6 * static_shadow_extra;

        static_len = max(static_len, (uint32_t)sizeof(cl_uint));

        if(static_len % expected_clear_kernel_size != 0)
        {
            int rem = static_len % expected_clear_kernel_size;

            static_len -= rem;
            static_len += expected_clear_kernel_size;
        }

        engine::g_static_shadow_light_buffer = compute::buffer(cl::context, static_len, CL_MEM_READ_WRITE, NULL);


        ///we clear the shadow buffer before using it anyway
        ///even if we didn't, we should use enqueuefillbuffer
        /*cl_uint* buf = (cl_uint*) clEnqueueMapBuffer(cl::cqueue.get(), engine::g_shadow_light_buffer.get(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, sizeof(cl_uint)*l_size*l_size*6*ln, 0, NULL, NULL, NULL);

        ///not sure how this pans out for stalling
        ///badly
        for(unsigned int i = 0; i<l_size*l_size*6*ln; i++)
        {
            buf[i] = UINT_MAX;
        }

        clEnqueueUnmapMemObject(cl::cqueue.get(), engine::g_shadow_light_buffer.get(), buf, 0, NULL, NULL);*/
    }

    if(old_dat)
    {
        dat.shadow_fragments_count = old_dat->shadow_fragments_count;
    }
    else
    {
        dat.shadow_fragments_count = new cl_uint[32];
    }

    dirty_shadow = false;
    dirty_gpu = false;

    return dat;
}

int light::get_num_shadowcasting_lights()
{
    int c = 0;

    for(auto& i : lightlist)
    {
        if(i->shadow == 1)
            c++;
    }

    return c;
}

int light::get_num_static_shadowcasters()
{
    int c = 0;

    for(auto& i : lightlist)
    {
        if(i->shadow == 1 && i->is_static)
        {
            c++;
        }
    }

    return c;
}

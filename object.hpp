#ifndef INCLUDED_HPP_OBJECT
#define INCLUDED_HPP_OBJECT

#include <vector>
#include "triangle.hpp"
#include <string>
#include <cl/cl.h>

#include <functional>

#include "obj_g_descriptor.hpp"
#include "object_context.hpp"
#include <vec/vec.hpp>

struct texture;

struct object_context_data;

template<typename T>
struct cache
{
    T old = T();
    T cur = T();
    cl_event old_ev;
    int init = 0;
};

struct texture_reference
{
    int id;
};

struct object
{
    std::string object_name;

    cl_uint gpu_tri_start;
    cl_uint gpu_tri_end;

    cl_int feature_flag;

    ///put the current texture id in here, as well as
    ///the texture addressing information
    ///allow the tex_gpu structure to have a .write(gpu/cpu_texture, object)
    ///this would probably have to update the cpu side texture
    ///as well as the member in the cpu texture array
    ///?
    ///restrict this to same sized texture updates, or subupdates with offsets/size

    cl_float4 pos;
    cl_float4 rot;
    quaternion rot_quat;
    float dynamic_scale = 1.f;

    cl_float4 centre; ///unused

    bool isactive;
    int tri_num;

    int isloaded;

    std::vector<triangle> tri_list;

    std::function<int (object*, cl_float4)> obj_vis;
    std::function<void (object*)> obj_load_func;

    ///remember to make this a generic vector of texture ids? Or can't due to opencl retardation?
    cl_uint tid; ///texture id ///on load
    cl_uint bid; ///bumpmap_id
    cl_uint rid;
    cl_uint ssid;

    cl_int object_g_id; ///obj_g_descriptor id ///assigned

    cl_uint has_bump; ///does this object have a bumpmap

    //cl_uint last_gpu_position_id = -1;

    ///despite being named similarly, these two are very different
    ///specular = 1.f - rough, diffuse = kD
    float specular;
    float spec_mult;
    float diffuse;
    //cl_uint two_sided;
    //cl_uint is_ss_reflective;

    cache<float> scale_cache;
    cache<cl_uint> tid_cache{UINT_MAX};
    cache<cl_int> feature_flag_cache;

    std::vector<cl_event> write_events;

    cl_uint unique_id;
    static cl_uint gid;

    bool gpu_writable;

    float position_quantise_grid_size = 1;
    bool position_quantise = false;

    object();
    ~object();

    void set_screenspace_map_id(cl_uint id);

    void set_active(bool param);
    void set_pos(cl_float4);
    void set_rot(cl_float4);
    void set_rot_quat(quaternion);
    void set_dynamic_scale(float _scale);
    void set_ss_reflective(int is_reflective);
    void set_two_sided(bool is_two_sided);
    void set_outlined(bool is_outlined);
    void set_is_static(bool is_static);
    void set_does_not_receive_dynamic_shadows(bool does_not_receive);
    void offset_pos(cl_float4);
    void swap_90();
    void swap_90_perp();
    void stretch(int dim, float amount);
    void scale(float);
    void set_quantise_position(bool do_quantise, float grid_size = 1);

    void destroy_textures(texture_context& tex_ctx);

    void patch_non_square_texture_maps(texture_context& ctx);
    void patch_non_2pow_texture_maps(texture_context& ctx);
    void patch_stretch_texture_to_full(texture_context& ctx);

    texture* get_texture();

    ///this is uncached for the moment
    cl_float4 get_centre();
    cl_float2 get_exact_height_bounds(); ///expensive!
    float get_min_y();

    void translate_centre(cl_float4);

    void set_vis_func(std::function<int (object*, cl_float4)>);
    int  call_vis_func(object*, cl_float4);

    void set_load_func(std::function<void (object*)>);
    void call_load_func(object*);

    void try_load(cl_float4); ///try and get the object, dependent on its visibility
    ///unused, probably removing visibility system due to complete infeasibility of automatic object loading based on anything useful

    void g_flush(object_context& cpu_dat, bool force = false); ///flush position (currently just) etc to gpu memory

    void set_buffer_offset(int offset);

    int buffer_offset = 0;

    ///for writing to gpu, need the memory to stick around
    private:
    cl_float8 posrot;

    cl_float4 last_pos;
    cl_float4 last_rot;
    cl_float4 last_rot_quat;

    cl_float4 cl_rot_quat;

    uint32_t last_object_context_data_id;
};




#endif

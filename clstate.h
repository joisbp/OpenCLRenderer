#ifndef CLSTATE_H_INCLUDED
#define CLSTATE_H_INCLUDED

#include <cl/opencl.h>

#include <gl/glew.h>

#include <boost/compute/source.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>

#include <string>
#include <utility>
#include <map>

///kernels and opencl stuff

namespace compute = boost::compute;


struct automatic_argument
{
    int pos = -1;
    std::string type;
    std::string identifier;
};

struct automatic_argument_identifiers
{
    std::string kernel_name;

    std::vector<automatic_argument> args;
};

struct kernel
{
    compute::kernel kernel;
    std::string name;
    bool loaded = false;
    cl_uint work_size;

    automatic_argument_identifiers automatic_arguments;
};

namespace cl
{
    void* map(compute::buffer& v, cl_map_flags flag, int size);
    void unmap(compute::buffer& v, void* ptr);
}

namespace cl
{
    extern compute::device device;
    extern compute::command_queue cqueue; ///rendering
    extern compute::command_queue cqueue2; ///build queue
    extern compute::command_queue cqueue_ooo; ///out of order write to gpu
    extern compute::context context;
    extern compute::program program;

    extern std::map<std::string, kernel> kernels;

    ///make this a map so that i can just directly call kernels
    extern kernel kernel1;
    extern kernel kernel2;
    extern kernel kernel3;
    extern kernel prearrange;
    extern kernel prearrange_light;
    extern kernel kernel1_light;
    extern kernel tile_clear;
    extern kernel clear_screen_buffer;
    extern kernel kernel1_oculus;
    extern kernel kernel2_oculus;
    extern kernel kernel3_oculus;
    extern kernel prearrange_oculus;
    extern kernel cloth_simulate;
    extern kernel point_cloud_depth;
    extern kernel point_cloud_recover;
    extern kernel space_dust;
    extern kernel space_dust_no_tile;
    extern kernel draw_ui;
    extern kernel draw_hologram;
    extern kernel blit_with_id;
    extern kernel blit_clear;
    extern kernel clear_id_buf;
    extern kernel clear_screen_dbuf;
    extern kernel draw_voxel_octree;
    extern kernel create_distortion_offset;
    extern kernel draw_fancy_projectile;
    extern kernel reproject_depth;
    extern kernel reproject_screen;
    extern kernel space_nebulae;
    extern kernel edge_smoothing;
    extern kernel shadowmap_smoothing_x;
    extern kernel shadowmap_smoothing_y;
    extern kernel raytrace;
    extern kernel render_voxels;
    extern kernel render_voxels_tex;
    extern kernel render_voxel_cube;
    extern kernel diffuse_unstable;
    extern kernel diffuse_unstable_tex;
    extern kernel advect;
    extern kernel advect_tex;
    extern kernel post_upscale;
    extern kernel warp_oculus;
    extern kernel goo_diffuse;
    extern kernel goo_advect;
    extern kernel fluid_amount;
    extern kernel update_boundary;
    extern kernel fluid_initialise_mem;
    extern kernel fluid_initialise_mem_3d;
    extern kernel fluid_timestep;
    extern kernel fluid_timestep_3d;
    extern kernel displace_fluid;
    extern kernel process_skins;
    extern kernel draw_hermite_skin;

    extern bool program_built;
    extern bool any_built_requested;
}



#endif // CLSTATE_H_INCLUDED

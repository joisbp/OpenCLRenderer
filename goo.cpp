#include "goo.hpp"

void update_boundary(compute::image3d& buf, cl_int bound, cl_uint global_ws[3], cl_uint local_ws[3])
{
    arg_list bound_args;
    bound_args.push_back(&buf);
    bound_args.push_back(&buf); ///undefined behaviour
    bound_args.push_back(&bound);

    run_kernel_with_list(cl::update_boundary, global_ws, local_ws, 3, bound_args);
}

void goo::tick(float dt)
{
    ///__kernel void advect(int width, int height, int depth, int b, __global float* d_out, __global float* d_in,
                            ///__global float* xvel, __global float* yvel, __global float* zvel, float dt)

    ///__kernel void diffuse_unstable(int width, int height, int depth, int b, __global float* x_out,
                            ///__global float* x_in, float diffuse, float dt)


    ///dens step

    cl_uint global_ws[3] = {width, height, depth};
    cl_uint local_ws[3] = {64, 2, 2};

    cl_int zero = 0;

    compute::buffer amount = compute::buffer(cl::context, sizeof(cl_int), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    cl_float gravity = 0.1f;//0.00000000001;

    float diffuse_const = 1;
    float dt_const = 0.01f;

    int next_dens = (n_dens + 1) % 2;
    int next_vel = (n_vel + 1) % 2;

    int bound = -1;

    arg_list dens_diffuse;
    dens_diffuse.push_back(&width);
    dens_diffuse.push_back(&height);
    dens_diffuse.push_back(&depth);
    dens_diffuse.push_back(&bound); ///unused
    dens_diffuse.push_back(&g_voxel[next_dens]); ///out
    dens_diffuse.push_back(&g_voxel[n_dens]); ///in
    dens_diffuse.push_back(&diffuse_const); ///temp
    dens_diffuse.push_back(&dt_const); ///temp

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    //update_boundary(g_voxel[next], -1, global_ws, local_ws);

    arg_list dens_advect;
    dens_advect.push_back(&width);
    dens_advect.push_back(&height);
    dens_advect.push_back(&depth);
    dens_advect.push_back(&bound); ///which boundary are we dealing with?
    dens_advect.push_back(&g_voxel[n_dens]); ///out
    dens_advect.push_back(&g_voxel[next_dens]); ///in
    dens_advect.push_back(&g_velocity_x[n_vel]); ///make float3
    dens_advect.push_back(&g_velocity_y[n_vel]);
    dens_advect.push_back(&g_velocity_z[n_vel]);
    dens_advect.push_back(&dt_const); ///temp
    dens_advect.push_back(&zero); ///float, but i believe ieee guarantees that 0 and 0 in float have the same representation

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    //update_boundary(g_voxel[n], -1, global_ws, local_ws);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_x[next_vel];
    dens_diffuse.args[5] = &g_velocity_x[n_vel];
    bound = 0;

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_y[next_vel];
    dens_diffuse.args[5] = &g_velocity_y[n_vel];
    bound = 1;

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    ///just modify relevant arguments
    dens_diffuse.args[4] = &g_velocity_z[next_vel];
    dens_diffuse.args[5] = &g_velocity_z[n_vel];
    bound = 2;

    run_kernel_with_list(cl::goo_diffuse, global_ws, local_ws, 3, dens_diffuse);

    //update_boundary(g_velocity_x[next], 0, global_ws, local_ws);
    //update_boundary(g_velocity_y[next], 1, global_ws, local_ws);
    //update_boundary(g_velocity_z[next], 2, global_ws, local_ws);

    ///nexts now valid
    dens_advect.args[4] = &g_velocity_x[n_vel];
    dens_advect.args[5] = &g_velocity_x[next_vel];
    dens_advect.args[10] = &zero;
    bound = 0;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_y[n_vel];
    dens_advect.args[5] = &g_velocity_y[next_vel];
    dens_advect.args[10] = &gravity;
    bound = 1;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    dens_advect.args[4] = &g_velocity_z[n_vel];
    dens_advect.args[5] = &g_velocity_z[next_vel];
    dens_advect.args[10] = &zero;
    bound = 2;

    run_kernel_with_list(cl::goo_advect, global_ws, local_ws, 3, dens_advect);

    //update_boundary(g_velocity_x[n], 0, global_ws, local_ws);
    //update_boundary(g_velocity_y[n], 1, global_ws, local_ws);
    //update_boundary(g_velocity_z[n], 2, global_ws, local_ws);

    arg_list fluid_count;
    fluid_count.push_back(&g_voxel[n_dens]);
    fluid_count.push_back(&amount);

    run_kernel_with_list(cl::fluid_amount, global_ws, local_ws, 3, fluid_count);

    cl_int readback;

    clEnqueueReadBuffer(cl::cqueue, amount.get(), CL_TRUE, 0, sizeof(cl_int), &readback, 0, NULL, NULL);

    printf("rback %d\n", readback);
}

template<int n, typename datatype>
void lattice<n, datatype>::init(int sw, int sh, int sd)
{
    which = 0;

    for(int i=0; i<n; i++)
    {
        in[i] = compute::buffer(cl::context, sizeof(datatype)*sw*sh*sd, CL_MEM_READ_WRITE, NULL);
        out[i] = compute::buffer(cl::context, sizeof(datatype)*sw*sh*sd, CL_MEM_READ_WRITE, NULL);
    }

    obstacles = compute::buffer(cl::context, sizeof(cl_uchar)*sw*sh*sd, CL_MEM_READ_WRITE, NULL);

    cl_uchar* buf = (cl_uchar*)cl::map(obstacles, CL_MAP_WRITE, sizeof(cl_uchar)*sw*sh*sd);

    for(int k=0; k<sd; k++)
    {
        for(int i=0; i<sh; i++)
        {
            for(int j=0; j<sw; j++)
            {
                ///not edge
                if(i != 0 && j != 0 && i != sh-1 && j != sw-1 && ((k != 0 && k != sd-1) || sd == 1))
                {
                    buf[k*sw*sh + i*sw + j] = 0;
                }
                else
                {
                    buf[k*sw*sh + i*sw + j] = 1;
                }
            }
        }
    }

    /*for(int i=10; i<sw-20; i++)
    {
        buf[50*sw*sh + 100*sw + i] = 1;
    }*/

    cl::unmap(obstacles, buf);

    cl_uint global_ws[3] = {sw,sh,1};

    if(n == 15)
        global_ws[2] = sd;

    cl_uint local_ws[3] = {1,1,1};

    local_ws[0] = 128;

    arg_list init_arg_list;

    for(int i=0; i<n; i++)
        init_arg_list.push_back(&in[i]);

    init_arg_list.push_back(&sw);
    init_arg_list.push_back(&sh);

    if(n == 15)
        init_arg_list.push_back(&sd);


    if(n == 9)
        run_kernel_with_list(cl::fluid_initialise_mem, global_ws, local_ws, 2, init_arg_list);
    if(n == 15)
        run_kernel_with_list(cl::fluid_initialise_mem_3d, global_ws, local_ws, 3, init_arg_list);


    screen = engine::gen_cl_gl_framebuffer_renderbuffer(&screen_id, sw, sh);

    width = sw;
    height = sh;
    depth = sd;
}


template<int n, typename datatype>
void lattice<n, datatype>::tick(compute::buffer* temp_obstacles)//compute::buffer skin[2], int& which_skin)
{
    compute::opengl_enqueue_acquire_gl_objects(1, &screen.get(), cl::cqueue);

    arg_list timestep;

    timestep.push_back(&obstacles);

    ///starting to get pretty hacky
    if(n == 9)
        timestep.push_back(temp_obstacles);

    ///???
    compute::buffer* cur_in = which == 0 ? in : out;
    compute::buffer* cur_out = which != 0 ? in : out;

    for(int i=0; i<n; i++)
    {
        timestep.push_back(&cur_out[i]);
    }

    for(int i=0; i<n; i++)
    {
        timestep.push_back(&cur_in[i]);
    }

    timestep.push_back(&width);
    timestep.push_back(&height);
    if(n == 15)
        timestep.push_back(&depth);

    timestep.push_back(&screen);

    //timestep.push_back(&skin[which_skin]);
    //timestep.push_back(&skin[(which_skin + 1) % 2]);

    cl_uint global_ws = width*height*depth;
    cl_uint local_ws = 128;


    if(n == 9)
        run_kernel_with_list(cl::fluid_timestep, &global_ws, &local_ws, 1, timestep);

    if(n == 15)
        run_kernel_with_list(cl::fluid_timestep_3d, &global_ws, &local_ws, 1, timestep);

    swap_buffers();

    //which_skin = (which_skin + 1) % 2;

    compute::opengl_enqueue_release_gl_objects(1, &screen.get(), cl::cqueue);
}

template<int n, typename datatype>
void lattice<n, datatype>::swap_buffers()
{
    current_out = which == 0 ? in : out;
    current_in = which != 0 ? in : out;

    which = !which;
}

template struct lattice<9, cl_float>;
template struct lattice<15, cl_float>;

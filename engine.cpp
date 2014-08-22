
#include <gl/glew.h>
#include "engine.hpp"
#include <math.h>
//

#include <gl/gl3.h>
#include <gl/glext.h>

#include "clstate.h"
#include <iostream>
#include <gl/gl.h>
#include "obj_mem_manager.hpp"
#include <stdio.h>
#include <limits.h>
#include "texture_manager.hpp"
#include "interact_manager.hpp" ///Separatation of church and state hlp
#include "text_handler.hpp"
#include "point_cloud.hpp"
#include "hologram.hpp"
#include "vec.hpp"
#include "ui_manager.hpp"

#define FOV_CONST 400.0f
///this needs changing

///opengl ids
unsigned int engine::gl_framebuffer_id=0;
unsigned int engine::gl_screen_id=0;


///the empty light buffer, and number of shadow lights
cl_uint* engine::blank_light_buf;
cl_uint engine::shadow_light_num;

///gpuside light buffer, and lightmap cubemap resolution
compute::buffer engine::g_shadow_light_buffer;
cl_uint engine::l_size;

cl_uint engine::height;
cl_uint engine::width;
cl_float4 engine::c_pos;
cl_float4 engine::c_rot;
cl_uint engine::depth;
cl_uint engine::g_size;

bool engine::camera_dirty;

int engine::nbuf=0; ///which depth buffer are we using?

compute::buffer engine::g_ui_id_screen;

float depth_far = 350000;

float idcalc(float value)
{
    return value * depth_far;
}

void engine::load(cl_uint pwidth, cl_uint pheight, cl_uint pdepth, std::string name)
{
    mdx = 0;
    mdy = 0;
    cmx = 0;
    cmy = 0;

    width=pwidth;
    height=pheight;
    depth=pdepth;

    blank_light_buf=NULL;

    cl_uint size = std::max(height, width);
    ///2^x=size;
    g_size=pow(2, ceil(log2(size)));
    ///this is the window size rounded to the nearest power of two

    l_size=1024; ///pass in as compilation parameter to opencl;

    shadow_light_num = 0;

    c_pos.x=0;
    c_pos.y=0;
    c_pos.z=0;

    c_rot.x=0;
    c_rot.y=0;
    c_rot.z=0;

    cl_float4 *blank = new cl_float4[g_size*g_size];
    memset(blank, 0, g_size*g_size*sizeof(cl_float4));

    cl_uint *arr = new cl_uint[g_size*g_size];
    memset(arr, UINT_MAX, g_size*g_size*sizeof(cl_uint));

    d_depth_buf = new cl_uint[g_size*g_size];

    ///opengl is the best, getting function ptrs
    PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");
    PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
    PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
    PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
    PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");

    ///generate and bind renderbuffers
    glGenRenderbuffersEXT(1, &gl_screen_id);
    glBindRenderbufferEXT(GL_RENDERBUFFER, gl_screen_id);

    ///generate storage for renderbuffer
    glRenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA, g_size, g_size);


    ///get a framebuffer and bind it
    glGenFramebuffersEXT(1, &gl_framebuffer_id);
    glBindFramebufferEXT(GL_FRAMEBUFFER, gl_framebuffer_id);


    ///attach one to the other
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_screen_id);


    ///have opencl nab this and store in g_screen
    g_screen = compute::opengl_renderbuffer(cl::context, gl_screen_id, compute::memory_object::write_only);


    ///this is a completely arbitrary size to store triangle uids in
    cl_uint size_of_uid_buffer = 40*1024*1024;
    cl_uint zero=0;

    ///camera position and rotation gpu side
    g_c_pos=compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_COPY_HOST_PTR, &c_pos);
    g_c_rot=compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE , NULL);




    ///change depth to be image2d_t ///not possible

    ///creates the two depth buffers and 2d triangle id buffer with size g_size, ie power of two closest to the screen resolution
    depth_buffer[0]=    compute::buffer(cl::context, sizeof(cl_uint)*g_size*g_size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);
    depth_buffer[1]=    compute::buffer(cl::context, sizeof(cl_uint)*g_size*g_size, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);

    g_tid_buf              = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);

    g_tid_buf_max_len      = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &size_of_uid_buffer);

    g_tid_buf_atomic_count = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    g_valid_fragment_num   = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    g_valid_fragment_mem   = compute::buffer(cl::context, size_of_uid_buffer*sizeof(cl_uint), CL_MEM_READ_WRITE, NULL);

    g_ui_id_screen         = compute::buffer(cl::context, width*height*sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, arr);

    ///length of the fragment buffer id thing, stored cpu side
    c_tid_buf_len = size_of_uid_buffer;

    ///number of lights
    obj_mem_manager::g_light_num=compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    g_shadow_light_buffer = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);

    compute::image_format format(CL_R, CL_UNSIGNED_INT32);
    ///screen ids as a uint32 texture
    g_id_screen_tex = compute::image2d(cl::context, CL_MEM_READ_WRITE, format, g_size, g_size, 0, NULL);

    g_distortion_buffer = compute::buffer(cl::context, sizeof(cl_float2)*width*height, 0, NULL);

    delete [] blank;

    //glEnable(GL_TEXTURE2D); ///?
}

void engine::realloc_light_gmem() ///for the moment, just reallocate everything
{
    cl_uint lnum=light::lightlist.size();

    ///turn pointer list into block of memory for writing to gpu
    std::vector<light> light_straight;
    for(int i=0; i<light::lightlist.size(); i++)
    {
        light_straight.push_back(*light::lightlist[i]);
    }

    ///gpu light memory
    obj_mem_manager::g_light_mem=compute::buffer(cl::context, sizeof(light)*lnum, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, light_straight.data());

    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_light_num, 0, sizeof(cl_uint), &lnum);

    ///sacrifice soul to chaos gods, allocate light buffers here

    int ln=0;

    ///doesnt work quite right
    ///needs to dirty if any light buffer is changed
    for(unsigned int i=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i]->shadow==1)
        {
            ln++;
        }
    }

    ///this is incorrect as per above comment
    if(shadow_light_num!=ln)
    {
        shadow_light_num=ln;

        ///blank cubemap filled with UINT_MAX
        blank_light_buf = new cl_uint[l_size*l_size*6*ln];
        memset(blank_light_buf, UINT_MAX, l_size*l_size*sizeof(cl_uint)*6*ln);

        g_shadow_light_buffer=compute::buffer(cl::context, sizeof(cl_uint)*l_size*l_size*6*ln, CL_MEM_READ_WRITE, NULL);

        for(int i=0; i<ln; i++)
        {
            cl::cqueue.enqueue_write_buffer(g_shadow_light_buffer, sizeof(cl_uint)*l_size*l_size*6*i, sizeof(cl_uint)*l_size*l_size*6, blank_light_buf);
        }

        delete [] blank_light_buf;
    }
}

light* engine::add_light(light* l)
{
    light* new_light = light::add_light(l);
    realloc_light_gmem();
    return new_light;
}

void engine::remove_light(light* l)
{
    light::remove_light(l); ///realloc light gmem or?
}

void engine::set_light_pos(light* l, cl_float4 position)
{
    l->pos.x = position.x;
    l->pos.y = position.y;
    l->pos.z = position.z;
}

void engine::g_flush_light(light* l) ///just position?
{
    ///flush light information to gpu
    int lid = light::get_light_id(l);
    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_light_mem, sizeof(light)*lid, sizeof(light), light::lightlist[lid]);
}

cl_float4 rot(double x, double y, double z, cl_float4 rotation)
{

    double i0x=x;
    double i0y=y;
    double i0z=z;

    double i1x=i0x;
    double i1y=i0y*cos(rotation.x) - sin(rotation.x)*i0z;
    double i1z=i0y*sin(rotation.x) + cos(rotation.x)*i0z;


    double i2x=i1x*cos(rotation.y) + i1z*sin(rotation.y);
    double i2y=i1y;
    double i2z=-i1x*sin(rotation.y) + i1z*cos(rotation.y);

    double i3x=i2x*cos(rotation.z) - i2y*sin(rotation.z);
    double i3y=i2x*sin(rotation.z) + i2y*cos(rotation.z);
    double i3z=i2z;

    cl_float4 ret;

    ret.x=i3x;
    ret.y=i3y;
    ret.z=i3z;

    return ret;
}

cl_float4 engine::rot_about(cl_float4 point, cl_float4 c_pos, cl_float4 c_rot)
{
    cl_float4 cos_rot;
    cos_rot.x = cos(c_rot.x);
    cos_rot.y = cos(c_rot.y);
    cos_rot.z = cos(c_rot.z);

    cl_float4 sin_rot;
    sin_rot.x = sin(c_rot.x);
    sin_rot.y = sin(c_rot.y);
    sin_rot.z = sin(c_rot.z);

    cl_float4 ret;
    ret.x=      cos_rot.y*(sin_rot.z+cos_rot.z*(point.x-c_pos.x))-sin_rot.y*(point.z-c_pos.z);
    ret.y=      sin_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))+cos_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.z=      cos_rot.x*(cos_rot.y*(point.z-c_pos.z)+sin_rot.y*(sin_rot.z*(point.y-c_pos.y)+cos_rot.z*(point.x-c_pos.x)))-sin_rot.x*(cos_rot.z*(point.y-c_pos.y)-sin_rot.z*(point.x-c_pos.x));
    ret.w = 0;

    return ret;
}

cl_float4 engine::rot_about_camera(cl_float4 val)
{
    return rot_about(val, c_pos, c_rot);
}

cl_float4 engine::back_project_about_camera(cl_float4 pos) ///from screenspace to worldspace
{
    cl_float4 local_position= {((pos.x - width/2.0f)*pos.z/FOV_CONST), ((pos.y - height/2.0f)*pos.z/FOV_CONST), pos.z, 0};


    cl_float4 zero = {0,0,0,0};

    ///backrotate pixel coordinate into globalspace
    cl_float4 global_position = local_position;

    global_position = rot_about(global_position,  zero, (cl_float4)
    {
        -c_rot.x, 0.0f, 0.0f, 0.0f
    });

    global_position        = rot_about(global_position, zero, (cl_float4)
    {
        0.0f, -c_rot.y, 0.0f, 0.0f
    });
    global_position        = rot_about(global_position, zero, (cl_float4)
    {
        0.0f, 0.0f, -c_rot.z, 0.0f
    });



    global_position.x += c_pos.x;
    global_position.y += c_pos.y;
    global_position.z += c_pos.z;

    global_position.w = 0;

    return global_position;
}

cl_float4 engine::rotate(cl_float4 pos, cl_float4 rot)
{
    cl_float4 zero = {0,0,0,0};

    return rot_about(pos, zero, rot);
}

cl_float4 engine::back_rotate(cl_float4 pos, cl_float4 rot)
{
    pos = rotate(pos, (cl_float4)
    {
        -rot.x, 0.0f, 0.0f, 0.0f
    });

    pos = rotate(pos, (cl_float4)
    {
        0.0f, -rot.y, 0.0f, 0.0f
    });
    pos = rotate(pos, (cl_float4)
    {
        0.0f, 0.0f, -rot.z, 0.0f
    });

    return pos;
}

cl_float4 depth_project_singular(cl_float4 rotated, int width, int height, float fovc)
{
    float rx;
    rx=(rotated.x) * (fovc/(rotated.z));
    float ry;
    ry=(rotated.y) * (fovc/(rotated.z));

    rx+=width/2.0f;
    ry+=height/2.0f;

    cl_float4 ret;

    ret.x = rx;
    ret.y = ry;
    ret.z = rotated.z;
    ret.w = 0;
    return ret;
}

cl_float4 engine::project(cl_float4 val)
{
    cl_float4 rotated = rot_about(val, c_pos, c_rot);

    cl_float4 projected = depth_project_singular(rotated, width, height, FOV_CONST);

    return projected;
}

int engine::get_mouse_delta_x()
{
    return mdx;
}

int engine::get_mouse_delta_y()
{
    return mdy;
}

void engine::input()
{
    int mx, my;

    mx = get_mouse_x();
    my = get_mouse_y();

    mdx = mx - cmx;
    mdy = my - cmy;

    cmx = mx;
    cmy = my;


    sf::Keyboard keyboard;

    static int distance_multiplier=1;

    ///handle camera input. Update to be based off frametime

    if(keyboard.isKeyPressed(sf::Keyboard::LShift))
    {
        distance_multiplier=10;
    }
    else
    {
        distance_multiplier=1;
    }

    double distance=0.04*distance_multiplier*30;

    if(keyboard.isKeyPressed(sf::Keyboard::W))
    {
        cl_float4 t=rot(0, 0, distance, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::S))
    {
        cl_float4 t=rot(0, 0, -distance, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::A))
    {
        cl_float4 t=rot(-distance, 0, 0, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::D))
    {
        cl_float4 t=rot(distance, 0, 0, c_rot);
        c_pos.x+=t.x;
        c_pos.y+=t.y;
        c_pos.z+=t.z;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::E))
    {
        c_pos.y-=0.04*distance_multiplier*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Q))
    {
        c_pos.y+=0.04*distance_multiplier*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Left))
    {
        c_rot.y-=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Right))
    {
        c_rot.y+=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Up))
    {
        c_rot.x-=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Down))
    {
        c_rot.x+=0.001*30;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::Escape))
    {
        window.close();
    }

    if(keyboard.isKeyPressed(sf::Keyboard::B))
    {
        std::cout << "rerr: " << c_pos.x << " " << c_pos.y << " " << c_pos.z << std::endl;
    }

    if(keyboard.isKeyPressed(sf::Keyboard::N))
    {
        std::cout << "rotation: " << c_rot.x << " " << c_rot.y << " " << c_rot.z << std::endl;
    }

    camera_dirty = true;
}



void engine::g_flush_camera()
{
    cl::cqueue.enqueue_write_buffer(g_c_pos, 0, sizeof(cl_float4), &c_pos);
    cl::cqueue.enqueue_write_buffer(g_c_rot, 0, sizeof(cl_float4), &c_rot);
}


void engine::construct_shadowmaps()
{
    cl_uint p1global_ws = obj_mem_manager::tri_num;
    cl_uint local=128;

    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }

    ///cubemap rotations
    cl_float4 r_struct[6];
    r_struct[0]=(cl_float4)
    {
        0.0,            0.0,            0.0,0.0
    };
    r_struct[1]=(cl_float4)
    {
        M_PI/2.0,       0.0,            0.0,0.0
    };
    r_struct[2]=(cl_float4)
    {
        0.0,            M_PI,           0.0,0.0
    };
    r_struct[3]=(cl_float4)
    {
        3.0*M_PI/2.0,   0.0,            0.0,0.0
    };
    r_struct[4]=(cl_float4)
    {
        0.0,            3.0*M_PI/2.0,   0.0,0.0
    };
    r_struct[5]=(cl_float4)
    {
        0.0,            M_PI/2.0,       0.0,0.0
    };

    cl_uint juan = 1;
    compute::buffer is_light = compute::buffer(cl::context, sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &juan);

    ///for every light, generate a cubemap for that light if its a light which casts a shadow
    for(unsigned int i=0, n=0; i<light::lightlist.size(); i++)
    {
        if(light::lightlist[i]->shadow==1)
        {
            for(int j=0; j<6; j++)
            {
                cl_uint zero = 0;

                compute::buffer l_pos;
                compute::buffer l_rot;
                cl_mem temp_l_mem;

                cl_buffer_region buf_reg;

                buf_reg.origin = n*sizeof(cl_uint)*l_size*l_size*6 + j*sizeof(cl_uint)*l_size*l_size;
                buf_reg.size   = sizeof(cl_uint)*l_size*l_size;

                l_pos = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &light::lightlist[i]->pos);
                l_rot = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &r_struct[j]);
                temp_l_mem = clCreateSubBuffer(g_shadow_light_buffer.get(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &buf_reg, NULL);

                compute::buffer l_mem(temp_l_mem, false);

                cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_num, 0, sizeof(cl_uint), &zero);

                //compute::buffer *prearglist[]={&obj_mem_manager::g_tri_mem, &obj_mem_manager::g_tri_num, &l_pos, &l_rot, &g_tid_buf, &g_tid_buf_max_len, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &is_light, &obj_mem_manager::g_obj_desc};

                arg_list prearg_list;

                prearg_list.push_back(&obj_mem_manager::g_tri_mem);
                prearg_list.push_back(&obj_mem_manager::g_tri_num);
                prearg_list.push_back(&light::lightlist[i]->pos);
                prearg_list.push_back(&r_struct[j]);
                prearg_list.push_back(&g_tid_buf);
                prearg_list.push_back(&g_tid_buf_max_len);
                prearg_list.push_back(&g_tid_buf_atomic_count);
                prearg_list.push_back(&obj_mem_manager::g_cut_tri_num);
                prearg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
                prearg_list.push_back(&juan);
                prearg_list.push_back(&obj_mem_manager::g_obj_desc);
                prearg_list.push_back(&g_distortion_buffer);


                run_kernel_with_list(cl::prearrange, &p1global_ws, &local, 1, prearg_list, true);


                cl_uint id_c = 0;


                cl::cqueue.enqueue_read_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &id_c);

                cl::cqueue.enqueue_write_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &zero);


                cl_uint p1global_ws_new = id_c;
                if(p1global_ws_new % local!=0)
                {
                    int rem=p1global_ws_new % local;
                    p1global_ws_new-=(rem);
                    p1global_ws_new+=local;
                }

                if(p1global_ws_new == 0)
                {
                    p1global_ws_new += local;
                }


                //compute::buffer *p1arglist[]= {&obj_mem_manager::g_tri_mem, &g_tid_buf, &obj_mem_manager::g_tri_num, &l_pos, &l_rot, &l_mem, &g_tid_buf_atomic_count, &obj_mem_manager::g_cut_tri_num, &obj_mem_manager::g_cut_tri_mem, &g_valid_fragment_num, &g_valid_fragment_mem, &is_light};

                arg_list p1arg_list;
                p1arg_list.push_back(&obj_mem_manager::g_tri_mem);
                p1arg_list.push_back(&g_tid_buf);
                p1arg_list.push_back(&obj_mem_manager::g_tri_num);
                p1arg_list.push_back(&l_mem);
                p1arg_list.push_back(&g_tid_buf_atomic_count);
                p1arg_list.push_back(&obj_mem_manager::g_cut_tri_num);
                p1arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
                p1arg_list.push_back(&g_valid_fragment_num);
                p1arg_list.push_back(&g_valid_fragment_mem);
                p1arg_list.push_back(&juan);
                p1arg_list.push_back(&g_distortion_buffer);

                run_kernel_with_list(cl::kernel1, &p1global_ws_new, &local, 1, p1arg_list, true);


                cl::cqueue.enqueue_write_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);
            }
            n++;
        }
    }
}

void engine::draw_galaxy_cloud(point_cloud_info& pc, compute::buffer& g_cam)
{
    ///__kernel void point_cloud(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot,
    ///__write_only image2d_t screen, __global uint* depth_buffer)

    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();


    compute::buffer screen_wrapper(g_screen.get(), true);

    compute::buffer *p1arglist[]={&pc.g_len, &pc.g_points_mem, &pc.g_colour_mem, &g_cam, &g_c_rot, &screen_wrapper, &depth_buffer[nbuf]};

    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;
    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }
    if(p1global_ws == 0)
    {
        p1global_ws += local;
    }

    run_kernel_with_args(cl::point_cloud_depth,   &p1global_ws, &local, 1, p1arglist, 7, true);
    run_kernel_with_args(cl::point_cloud_recover, &p1global_ws, &local, 1, p1arglist, 7, true);


    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}

void engine::draw_space_dust_cloud(point_cloud_info& pc, compute::buffer& g_cam)
{
    ///__kernel void space_dust(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot,
    ///__write_only image2d_t screen, __global uint* depth_buffer)


    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();


    compute::buffer screen_wrapper(g_screen.get(), true);

    compute::buffer *p1arglist[]={&pc.g_len, &pc.g_points_mem, &pc.g_colour_mem, &g_cam, &g_c_pos, &g_c_rot, &screen_wrapper, &depth_buffer[nbuf]};

    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;
    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }

    if(p1global_ws == 0)
    {
        p1global_ws += local;
    }

    run_kernel_with_args(cl::space_dust, &p1global_ws, &local, 1, p1arglist, 8, true);


    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}

///merge with above kernel?
void engine::draw_space_dust_no_tile(point_cloud_info& pc, compute::buffer& offset_pos)
{
    ///__kernel void space_dust(__global uint* num, __global float4* positions, __global uint* colours, __global float4* c_pos, __global float4* c_rot,
    ///__write_only image2d_t screen, __global uint* depth_buffer)


    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();


    compute::buffer screen_wrapper(g_screen.get(), true);

    compute::buffer *p1arglist[]={&pc.g_len, &pc.g_points_mem, &pc.g_colour_mem, &offset_pos, &g_c_pos, &g_c_rot, &screen_wrapper, &depth_buffer[nbuf]};

    cl_uint local = 128;

    cl_uint p1global_ws = pc.len;

    run_kernel_with_args(cl::space_dust_no_tile, &p1global_ws, &local, 1, p1arglist, 8, true);

    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}



///this function is horrible and needs to be reworked into multiple smaller functions
void engine::draw_bulk_objs_n()
{
    if(camera_dirty)
        g_flush_camera();

    static bool argd = true;

    sf::Clock start;


    cl_uint zero=0;

    ///this is not a shadowmapping kernel. This needs to be passed in as a compile time parameter
    compute::buffer is_light(cl::context,  sizeof(cl_uint), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &zero);


    cl_uint p3global_ws[]= {width, height};
    cl_uint p3local_ws[]= {16, 8};


    ///need a better way to clear light buffer

    sf::Clock c;

    cl_mem scr = g_screen.get();
    ///acquire opengl objects for opencl
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);

    cl::cqueue.finish();

    ///1 thread per triangle
    cl_uint p1global_ws = obj_mem_manager::tri_num;
    cl_uint local=128;

    if(p1global_ws % local!=0)
    {
        int rem=p1global_ws % local;
        p1global_ws-=(rem);
        p1global_ws+=local;
    }

    ///clear the number of triangles that are generated after first kernel run
    cl::cqueue.enqueue_write_buffer(obj_mem_manager::g_cut_tri_num, 0, sizeof(cl_uint), &zero);

    cl_float4 test_pos = {0,400,0};
    int distort_num = 1;

    compute::buffer distorts = compute::buffer(cl::context, sizeof(cl_float4), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &test_pos);

    arg_list distort_arg_list;
    distort_arg_list.push_back(&distorts);
    distort_arg_list.push_back(&distort_num);
    distort_arg_list.push_back(&c_pos);
    distort_arg_list.push_back(&c_rot);
    distort_arg_list.push_back(&g_distortion_buffer);

    run_kernel_with_list(cl::create_distortion_offset, p3global_ws, p3local_ws, 2, distort_arg_list, true, false);


    arg_list prearg_list;

    prearg_list.push_back(&obj_mem_manager::g_tri_mem);
    prearg_list.push_back(&obj_mem_manager::g_tri_num);
    prearg_list.push_back(&c_pos);
    prearg_list.push_back(&c_rot);
    prearg_list.push_back(&g_tid_buf);
    prearg_list.push_back(&g_tid_buf_max_len);
    prearg_list.push_back(&g_tid_buf_atomic_count);
    prearg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    prearg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    prearg_list.push_back(&zero);
    prearg_list.push_back(&obj_mem_manager::g_obj_desc);
    prearg_list.push_back(&g_distortion_buffer);

    run_kernel_with_list(cl::prearrange, &p1global_ws, &local, 1, prearg_list, true, false);

    //std::cout << "ptime " << c.getElapsedTime().asMicroseconds() << std::endl;

    sf::Clock p1;

    cl_uint id_c = 0;

    ///read back number of fragments
    cl::cqueue.enqueue_read_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &id_c);

    ///clear number of valid fragments
    cl::cqueue.enqueue_write_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &zero);

    ///round global args to multiple of local work size
    cl_uint p1global_ws_new = id_c;
    if(p1global_ws_new % local!=0)
    {
        int rem=p1global_ws_new % local;
        p1global_ws_new-=(rem);
        p1global_ws_new+=local;
    }

    if(p1global_ws_new == 0)
    {
        p1global_ws_new += local;
    }

    ///write depth of triangles to buffer, ie z buffering

    arg_list p1arg_list;
    p1arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p1arg_list.push_back(&g_tid_buf);
    p1arg_list.push_back(&obj_mem_manager::g_tri_num);
    p1arg_list.push_back(&depth_buffer[nbuf]);
    p1arg_list.push_back(&g_tid_buf_atomic_count);
    p1arg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    p1arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    p1arg_list.push_back(&g_valid_fragment_num);
    p1arg_list.push_back(&g_valid_fragment_mem);
    p1arg_list.push_back(&zero);
    p1arg_list.push_back(&g_distortion_buffer);

    run_kernel_with_list(cl::kernel1, &p1global_ws_new, &local, 1, p1arg_list, true, false);

    sf::Clock p2;
    int valid_tri_num = 0;

    cl::cqueue.enqueue_read_buffer(g_valid_fragment_num, 0, sizeof(cl_uint), &valid_tri_num);

    cl_uint p2global_ws = valid_tri_num;

    cl_uint local2=128;

    if(p2global_ws % local2!=0)
    {
        int rem=p2global_ws % local2;
        p2global_ws-=(rem);
        p2global_ws+=local2;
    }

    if(p2global_ws == 0)
    {
        p2global_ws += local2;
    }

    compute::buffer image_wrapper(g_id_screen_tex.get(), true);

    ///recover ids from z buffer by redoing previous step, this could be changed by using 2d atomic map to merge the kernels

    arg_list p2arg_list;
    p2arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p2arg_list.push_back(&g_tid_buf);
    p2arg_list.push_back(&obj_mem_manager::g_tri_num);
    p2arg_list.push_back(&depth_buffer[nbuf]);
    p2arg_list.push_back(&image_wrapper);
    p2arg_list.push_back(&g_tid_buf_atomic_count);
    p2arg_list.push_back(&obj_mem_manager::g_cut_tri_num);
    p2arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    p2arg_list.push_back(&g_valid_fragment_num);
    p2arg_list.push_back(&g_valid_fragment_mem);
    p2arg_list.push_back(&g_distortion_buffer);



    run_kernel_with_list(cl::kernel2, &p2global_ws, &local, 1, p2arg_list, true, false);


    sf::Clock c3;

    cl::cqueue.enqueue_write_buffer(g_tid_buf_atomic_count, 0, sizeof(cl_uint), &zero);




    int nnbuf = (nbuf + 1) % 2;


    compute::buffer screen_wrapper(g_screen.get(), true);

    compute::buffer texture_wrapper(texture_manager::g_texture_array.get());

    /// many arguments later

    arg_list p3arg_list;
    p3arg_list.push_back(&obj_mem_manager::g_tri_mem);
    p3arg_list.push_back(&obj_mem_manager::g_tri_num);
    p3arg_list.push_back(&c_pos);
    p3arg_list.push_back(&c_rot);
    p3arg_list.push_back(&depth_buffer[nbuf]);
    p3arg_list.push_back(&image_wrapper);
    p3arg_list.push_back(&texture_wrapper);
    p3arg_list.push_back(&screen_wrapper);
    p3arg_list.push_back(&texture_manager::g_texture_numbers);
    p3arg_list.push_back(&texture_manager::g_texture_sizes);
    p3arg_list.push_back(&obj_mem_manager::g_obj_desc);
    p3arg_list.push_back(&obj_mem_manager::g_obj_num);
    p3arg_list.push_back(&obj_mem_manager::g_light_num);
    p3arg_list.push_back(&obj_mem_manager::g_light_mem);
    p3arg_list.push_back(&g_shadow_light_buffer);
    p3arg_list.push_back(&depth_buffer[nnbuf]);
    p3arg_list.push_back(&g_tid_buf);
    p3arg_list.push_back(&obj_mem_manager::g_cut_tri_mem);
    p3arg_list.push_back(&g_distortion_buffer);

    ///this is the deferred screenspace pass
    run_kernel_with_list(cl::kernel3, p3global_ws, p3local_ws, 2, p3arg_list, true, false);



    #ifdef DEBUGGING
    //clEnqueueReadBuffer(cl::cqueue, depth_buffer[nbuf], CL_TRUE, 0, sizeof(cl_uint)*g_size*g_size, d_depth_buf, 0, NULL, NULL);
    #endif

    ///release opengl stuff
    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();

    camera_dirty = false;
}

///never going to work, would have to reproject?
void engine::draw_ui()
{
    compute::buffer screen_wrapper(g_screen.get(), true);

    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();


    cl_uint global_ws = obj_mem_manager::obj_num;

    cl_uint local2=256;


    compute::buffer wrap(scr);

    compute::buffer* ui_args[] = {&obj_mem_manager::g_obj_desc, &obj_mem_manager::g_obj_num, &wrap, &g_c_pos, &g_c_rot};

    run_kernel_with_args(cl::draw_ui, &global_ws, &local2, 1, ui_args, 5, true);


    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}

template<typename T, typename Q>
float vmin(T t, Q q)
{
    return std::min(t, (T)q);
}

template<typename T, typename... Args>
float vmin(T t, Args... args)
{
    return std::min(t, vmin(args...));
}

template<typename T, typename Q>
float vmax(T t, Q q)
{
    return std::max(t, (T)q);
}

template<typename T, typename... Args>
float vmax(T t, Args... args)
{
    return std::max(t, vmax(args...));
}

bool within_bounds(float v, float min, float max)
{
    return v >= min && v < max;
}

void engine::draw_holograms()
{
    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();

    for(int i=0; i<hologram_manager::tex_id.size(); i++)
    {
        ///figure out parent shit
        if(hologram_manager::parents[i] == NULL) ///?
            continue;

        cl_float4 parent_pos = hologram_manager::parents[i]->pos;
        cl_float4 parent_rot = hologram_manager::parents[i]->rot;

        cl_float4 pos = hologram_manager::positions[i];
        cl_float4 rot = hologram_manager::rotations[i];

        float w = hologram_manager::tex_size[i].first * hologram_manager::scales[i];
        float h = hologram_manager::tex_size[i].second * hologram_manager::scales[i];

        cl_float4 tl_rot = rot_about({-w/2, -h/2,  0, 0}, {0,0,0,0}, rot);
        tl_rot = add(tl_rot, pos);
        tl_rot = rot_about(tl_rot, {0,0,0,0}, parent_rot);
        tl_rot = add(tl_rot, parent_pos);
        cl_float4 tl_projected = project(tl_rot);

        cl_float4 tr_rot = rot_about({w/2, -h/2,  0, 0}, {0,0,0,0}, rot);
        tr_rot = add(tr_rot, pos);
        tr_rot = rot_about(tr_rot, {0,0,0,0}, parent_rot);
        tr_rot = add(tr_rot, parent_pos);
        cl_float4 tr_projected = project(tr_rot);

        cl_float4 br_rot = rot_about({w/2, h/2,  0, 0}, {0,0,0,0}, rot);
        br_rot = add(br_rot, pos);
        br_rot = rot_about(br_rot, {0,0,0,0}, parent_rot);
        br_rot = add(br_rot, parent_pos);
        cl_float4 br_projected = project(br_rot);

        cl_float4 bl_rot = rot_about({-w/2, h/2,  0, 0}, {0,0,0,0}, rot);
        bl_rot = add(bl_rot, pos);
        bl_rot = rot_about(bl_rot, {0,0,0,0}, parent_rot);
        bl_rot = add(bl_rot, parent_pos);
        cl_float4 bl_projected = project(bl_rot);

        float minx = vmin(tl_projected.x, bl_projected.x, br_projected.x, tr_projected.x);
        float maxx = vmax(tl_projected.x, bl_projected.x, br_projected.x, tr_projected.x);

        float miny = vmin(tl_projected.y, bl_projected.y, br_projected.y, tr_projected.y);
        float maxy = vmax(tl_projected.y, bl_projected.y, br_projected.y, tr_projected.y);

        ///remove massive out of bounds check

        int g_w = ceil(maxx - minx);
        int g_h = ceil(maxy - miny);

        minx = vmax(minx, 0.0f);
        maxx = vmin(maxx, width - 1);
        miny = vmax(miny, 0.0f);
        maxy = vmin(maxy, height - 1);

        if(g_h <= 1 || g_w <= 1)
            continue;


        bool bounds = (!within_bounds(minx, 0, width) && !within_bounds(maxx, 0, width)) || (!within_bounds(miny, 0, height) && !within_bounds(maxy, 0, height));

        ///if all of x or all of y out of bounds return
        if(bounds)
            continue;

        ///need to pass in minx, maxy;
        //if(g_w >= width || g_h >= height)
        //    continue;

        if(g_w >= width)
            g_w = width - 1;

        if(g_h >= height)
            g_h = height - 1;

        if(bl_projected.z < 20 || br_projected.z < 20 || tl_projected.z < 20 || tr_projected.z < 20)
            continue;

        cl_float4 points[4] = {tr_projected, br_projected, bl_projected, tl_projected};

        //printf("T %d %d\n",g_w,g_h); ///yay actually correct!
        ///invoke kernel with this and back project nearest etc

        hologram_manager::acquire(i);

        compute::buffer wrap_scr(scr);
        compute::buffer wrap_tex(hologram_manager::g_tex_mem[i]);

        compute::buffer g_br_pos = compute::buffer(cl::context, sizeof(cl_float4)*4, CL_MEM_COPY_HOST_PTR, &points);

        compute::buffer position_wrap = compute::buffer(hologram_manager::g_positions[i]);
        compute::buffer rotation_wrap = compute::buffer(hologram_manager::g_rotations[i]);

        compute::buffer scale_wrap = compute::buffer(hologram_manager::g_scales[i]);

        compute::buffer id_wrap = compute::buffer(hologram_manager::g_id_bufs[i]);

        cl_float8 posrot;
        posrot.lo = parent_pos;
        posrot.hi = parent_rot;

        compute::buffer g_posrot = compute::buffer(cl::context, sizeof(cl_float8), CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY, &posrot);

        compute::buffer* holo_args[] = {&wrap_tex, &g_posrot, &g_br_pos, &position_wrap, &rotation_wrap, &g_c_pos, &g_c_rot, &wrap_scr, &scale_wrap, &depth_buffer[nbuf], &id_wrap, &g_ui_id_screen};

        cl_uint num[2] = {(cl_uint)g_w, (cl_uint)g_h};

        cl_uint ls[2] = {16, 16};

        run_kernel_with_args(cl::draw_hologram, num, ls, 2, holo_args, 12, true);

        hologram_manager::release(i);
    }

    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}

void engine::draw_voxel_octree(g_voxel_info& info)
{
    cl_mem scr = g_screen.get();
    compute::opengl_enqueue_acquire_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();


    compute::buffer screen_wrap = compute::buffer(scr, true);

    compute::buffer* argv[] = {&screen_wrap, &info.g_voxel_mem, &g_c_pos, &g_c_rot};

    cl_uint glob[] = {window.getSize().x, window.getSize().y};
    cl_uint local[] = {16, 16};

    run_kernel_with_args(cl::draw_voxel_octree, glob, local, 2, argv, 4, true);


    compute::opengl_enqueue_release_gl_objects(1, &scr, cl::cqueue);
    cl::cqueue.finish();
}

void engine::render_buffers()
{
    if(camera_dirty)
        g_flush_camera();

    sf::Clock clk;
    //draw_ui(); ///?
    //std::cout << "UI stack time: " << clk.getElapsedTime().asMicroseconds() << std::endl;

    PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC)wglGetProcAddress("glBindFramebufferEXT");

    PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC)wglGetProcAddress("glBlitFramebufferEXT");

    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, gl_framebuffer_id);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

    ///blit buffer to screen
    glBlitFramebufferEXT(0 , 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glFinish();

    interact::deplete_stack();
    interact::clear();


    text_handler::render();

    window.display();


    ///swap depth buffers
    nbuf++;
    nbuf = nbuf % 2;

    camera_dirty = false;
}

///does this need to be somewhere more fun? Minimap vs UI
void engine::ui_interaction()
{
    int mx = mouse.getPosition(window).x;
    int my = height - mouse.getPosition(window).y;

    int selected = ui_manager::selected_value;

    ///ui element
    if(selected != -1 && selected >= 0 && selected < ui_manager::ui_elems.size() && !(selected & MINIMAP_BITFLAG))
    {
        ui_element& e = *ui_manager::ui_elems[selected];

        float depth = -1;

        cl_uint duint;

        cl::cqueue.enqueue_read_buffer(depth_buffer[nbuf], sizeof(cl_uint)*(my*width + mx), sizeof(cl_uint), &duint);

        if(duint != UINT_MAX && duint != 0)
        {
            depth = idcalc((float)duint/UINT_MAX);

            cl_float4 unproj_pos = {(mx - width/2.0f)*depth/FOV_CONST, (my - height/2.0f)*depth/FOV_CONST, depth, 0.0f};

            cl_float4 world_pos = back_rotate(unproj_pos, c_rot);
            world_pos = add(world_pos, c_pos);

            int id = e.ref_id;

            int real = hologram_manager::get_real_id(id);

            objects_container* parent = hologram_manager::parents[real];

            cl_float4 ppos = parent->pos;
            cl_float4 prot = parent->rot;

            cl_float4 unparent = sub(world_pos, ppos);
            unparent = back_rotate(unparent, prot);

            cl_float4 upos = hologram_manager::positions[real];
            cl_float4 urot = hologram_manager::rotations[real];

            cl_float4 unui = sub(unparent, upos);
            unui = back_rotate(unui, urot);

            float x = unui.x;
            float y = unui.y;

            float scale = hologram_manager::scales[real];

            float w = hologram_manager::tex_size[real].first;
            float h = hologram_manager::tex_size[real].second;


            x /= scale;
            y /= scale;

            float px = x + w/2.0f;
            float py = y + h/2.0f;

            int tw = e.w;
            int th = e.h;

            if(!(px < 0 || px >= w || h - (int)py < 0 || h - (int)py >= h))
            {
                e.finish.x = px;
                e.finish.y = (h - py);
            }
        }
    }
}

int engine::get_mouse_x()
{
    return mouse.getPosition(window).x;
}

int engine::get_mouse_y()
{
    return mouse.getPosition(window).y;
}

void engine::set_camera_pos(cl_float4 p)
{
    c_pos = p;
    camera_dirty = true;
}

void engine::set_camera_rot(cl_float4 r)
{
    c_rot = r;
    camera_dirty = true;
}

///unused, and due to change in plans may be removed
void engine::check_obj_visibility()
{
    for(unsigned int i=0; i<objects_container::obj_container_list.size(); i++)
    {
        objects_container *T = objects_container::obj_container_list[i];
        //T->call_obj_vis_load(c_pos);
        for(int j=0; j<T->objs.size(); j++)
        {
            if(!T->objs[j].isloaded && T->objs[j].call_vis_func(&T->objs[j], c_pos))
            {
                ///fire g_arrange_mem asynchronously
                std::cout << "hi" << std::endl;
                obj_mem_manager::g_arrange_mem();
                obj_mem_manager::g_changeover();
                return;
            }
        }
    }
}

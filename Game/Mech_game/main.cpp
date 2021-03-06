#include "../../hologram.hpp"
#include "../../proj.hpp"
#include "../../ocl.h"
#include "../../texture_manager.hpp"
#include "../newtonian_body.hpp"
#include "../collision.hpp"
#include "../../interact_manager.hpp"
#include "../game_object.hpp"

#include "../../text_handler.hpp"
#include <sstream>
#include <string>
#include "../../vec.hpp"

#include "../galaxy/galaxy.hpp"

#include "../game_manager.hpp"
#include "../space_dust.hpp"
#include "../asteroid/asteroid_gen.hpp"
#include "../../ui_manager.hpp"

#include "../ship.hpp"
#include "../../terrain_gen/terrain_gen.hpp"

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    object_context context;

    objects_container& sponza = *context.make_new();

    //sponza.set_file("sp2/sp2.obj");
    //sponza.set_file("Objects/pre-ruin.obj");
    sponza.set_load_func(std::bind(create_terrain, std::placeholders::_1, 1000, 2000));

    sponza.set_active(true);
    sponza.cache = false;

    objects_container& c1 = *context.make_new();
    c1.set_file("../../objects/cylinder.obj");
    c1.set_pos({-2000, 0, 0});
    c1.set_active(true);

    objects_container& c2 = *context.make_new();
    c2.set_file("../../objects/cylinder.obj");
    c2.set_pos({0,0,0});
    c2.set_active(true);

    engine window;
    window.load(1280,768,1000, "turtles", "../../cl2.cl", true);

    window.set_camera_pos((cl_float4){0,0,0,0});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures only get generated once, (potentially) in parallel on cpu?

    //obj_mem_manager::load_active_objects();

    context.load_active();

    //sponza.scale(100.0f);

    c1.scale(100.0f);
    c2.scale(100.0f);

    c1.set_rot({M_PI/2.0f, 0, 0});
    c2.set_rot({M_PI/2.0f, 0, 0});

    texture_manager::allocate_textures();
    auto tex_gpu = texture_manager::build_descriptors();

    window.set_tex_data(tex_gpu);

    context.build();

    auto ctx = context.fetch();
    window.set_object_data(*ctx);

    /*texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();*/

    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    //l.set_shadow_bright(1, 1);
    l.set_shadow_casting(0);
    l.set_brightness(1);
    //l.set_pos((cl_float4){-150, 150, 0});
    l.set_pos((cl_float4){4000, 4000, 5000});
    //l.set_pos((cl_float4){-200, 2000, -100, 0});
    //l.set_pos((cl_float4){-200, 200, -100, 0});
    //l.set_pos((cl_float4){-400, 150, -555, 0});
    //window.add_light(&l);

    light::add_light(&l);

    auto light_data = light::build();

    window.set_light_data(light_data);

    //l.set_pos((cl_float4){0, 200, -450, 0});
    l.set_pos((cl_float4){-1200, 150, 0, 0});
    l.shadow=0;

    //window.add_light(&l);

    window.construct_shadowmaps();

    while(window.window.isOpen())
    {
        sf::Clock c;

        while(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        //window.input();

        auto event = window.draw_bulk_objs_n();

        //window.render_buffers();
        window.blit_to_screen();
        window.flip();

        window.set_render_event(event);
        window.render_block();

        //window.display();

        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}

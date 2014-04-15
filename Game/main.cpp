#include "../proj.hpp"
#include "../ocl.h"
#include "../texture_manager.hpp"
#include "newtonian_body.hpp"
#include "collision.hpp"
#include "../interact_manager.hpp"
#include "game_object.hpp"
///todo eventually
///split into dynamic and static objects

///todo
///fix memory management to not be atrocious


///fix into different runtime classes - specify ship attributes as vec

int main(int argc, char *argv[])
{
    ///remember to make g_arrange_mem run faster!

    //objects_container second_ship;

    //second_ship.set_file("../objects/shittyspaceship.obj");
    //second_ship.set_file("../objects/ship_with_uvs_2.obj");
    //second_ship.set_active(true);

    //second_ship.translate_centre((cl_float4){400,0,0,0});

    game_object ship;
    ship.set_file("../objects/shittyspaceship.obj");
    ship.set_active(true);

    ship.add_transform(TRANSLATE, (cl_float4){400,0,0,0});
    ship.add_transform(ROTATE90);

    game_object ship2;
    ship2.set_file("../objects/shittyspaceship.obj");
    ship2.set_active(true);

    ship2.add_transform(TRANSLATE, (cl_float4){400,0,0,0});
    ship2.add_transform(ROTATE90);


    engine window;
    window.window.create(sf::VideoMode(800, 600), "fixthisfixthisfixthis");
    oclstuff("../cl2.cl");
    window.load(800,600,1000, "turtles");

    interact::set_render_window(&window.window);

    window.set_camera_pos((cl_float4){-800,150,-570});

    ///write a opencl kernel to generate mipmaps because it is ungodly slow?
    ///Or is this important because textures 1gen?

    obj_mem_manager::load_active_objects();


    ship.process_transformations();
    ship.calc_push_physics_info(); ///separate into separate pos call?

    ship2.process_transformations();
    ship2.calc_push_physics_info((cl_float4){0,-200,0,0});


    texture_manager::allocate_textures();

    obj_mem_manager::g_arrange_mem();
    obj_mem_manager::g_changeover();



    sf::Event Event;

    light l;
    l.set_col((cl_float4){1.0, 1.0, 1.0, 0});
    l.set_shadow_casting(1);
    l.set_brightness(1.0f);
    l.set_pos((cl_float4){2000, 200, -100, 0});
    l.set_type(0.0f);
    l.shadow = 0;
    light* start_light = window.add_light(&l);

    //l.pos.w = 0.0f;

    window.add_light(&l);

    window.construct_shadowmaps();


    newtonian_body* player_ship = ship.get_newtonian();


    //newtonian_body l1;
    //l1.obj = NULL;
    //l1.linear_momentum = (cl_float4){50, 0, 0, 0};
    //l1.position = (cl_float4){0,200,0,0};

    //newtonian_body& light_newt = *l1.push_laser(start_light);

    bool lastp = false;

    while(window.window.isOpen())
    {
        sf::Clock c;

        if(window.window.pollEvent(Event))
        {
            if(Event.type == sf::Event::Closed)
                window.window.close();
        }

        window.input();

        window.draw_bulk_objs_n();

        window.render_buffers();

        player_ship->set_rotation_direction((cl_float4){0,0,0,0});
        player_ship->set_linear_force_direction((cl_float4){0,0,0,0});


        sf::Keyboard k;
        if(k.isKeyPressed(sf::Keyboard::I))
        {
            player_ship->set_linear_force_direction((cl_float4){1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::J))
        {
            player_ship->set_linear_force_direction((cl_float4){0, 0, 1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::K))
        {
            player_ship->set_linear_force_direction((cl_float4){-1, 0, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::L))
        {
            player_ship->set_linear_force_direction((cl_float4){0, 0, -1, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::O))
        {
            player_ship->set_rotation_direction((cl_float4){0, -1, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::U))
        {
            player_ship->set_rotation_direction((cl_float4){0, 1, 0, 0});
        }
        if(k.isKeyPressed(sf::Keyboard::P) && !lastp)
        {
            player_ship->fire();
            lastp = true;
        }
        if(!k.isKeyPressed(sf::Keyboard::P))
        {
            lastp = false;
        }

        //ship.tick(c.getElapsedTime().asMicroseconds()/1000.0);

        newtonian_manager::tick_all(c.getElapsedTime().asMicroseconds()/1000.0);
        newtonian_manager::collide_lasers_with_ships();

        for(int i=0; i<newtonian_manager::body_list.size(); i++)
        {
            newtonian_body* b = newtonian_manager::body_list[i];
            if(b->type==0)
                b->obj->g_flush_objects();
            //if(b->type==1)
            //    window.g_flush_light(b->laser);

            window.realloc_light_gmem();
        }

        newtonian_manager::draw_all_box();


        std::cout << c.getElapsedTime().asMicroseconds() << std::endl;
    }
}

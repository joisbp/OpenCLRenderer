#include <iostream>
#include <sfml/graphics.hpp>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cl/cl.h>

#include "galaxy.hpp"
#include "../../vec.hpp"

using namespace std;

#define RADIUS 200.0f


#define MAP_RESOLUTION 9046

float func(float theta)
{
    const float A = RADIUS;
    const float B = 0.5;
    const float N = 4.0f;
    return A / log(B*tan(theta/(2*N)));
}

bool in_bound(int x, int y)
{
    if(x < 0 || x >= MAP_RESOLUTION || y < 0 || y >= MAP_RESOLUTION)
        return false;

    return true;
}

struct star_info
{
    float x, y;
    int type;
    float brightness;
};

inline float randf()
{
    return (float)rand() / RAND_MAX;
}

void different_scatter(int x, int y, vector<star_info>& vals, float dist, int n, float dist_factor, float brightness = 1, int type = 1)
{
    for(int i=0; i<n; i++)
    {
        float rand_angle = (float)rand()/RAND_MAX;
        rand_angle *= M_PI*2.0f;

        float rdist = dist;
        rdist *= dist_factor;

        //rdist = max(1.0f, rdist - dist*dist/RADIUS*RADIUS);

        float px = 1.0/rdist*cos(rand_angle) + x;
        float py = 1.0/rdist*sin(rand_angle) + y;

        //if(in_bound(px, py))
        //    vals[(int)py][(int)px] = 1;

        vals.push_back({px, py, type, brightness});

    }
}

void random_points_less_edge(vector<star_info>& vals, int num, float mult_fact, float brightness = 1, bool is_random_z = false, int type = 1)
{
    for(int i=0; i<num; i++)
    {
        float rad = 0.4f + ((float)rand() / RAND_MAX);

        rad*=rad*rad;
        rad*=RADIUS*mult_fact;

        float angle = (float)rand() / RAND_MAX;

        angle *= M_PI*2;

        float px = rad*cos(angle);
        float py = rad*sin(angle);

        px += MAP_RESOLUTION/2.0f;
        py += MAP_RESOLUTION/2.0f;

        if(is_random_z)
            type = 3;

        //if(in_bound(px, py))
        //    vals[(int)py][(int)px] = type;

        vals.push_back({px, py, type, brightness});

    }
}

void random_points(vector<star_info>& vals, int num, float mult_fact, float brightness = 1, bool is_random_z = false)
{
    for(int i=0; i<num; i++)
    {
        float rad = (float)rand() / RAND_MAX;

        rad *= RADIUS;
        rad *= mult_fact;

        float angle = (float)rand() / RAND_MAX;

        angle *= M_PI*2;

        float px = rad*cos(angle);
        float py = rad*sin(angle);

        px += MAP_RESOLUTION/2.0f;
        py += MAP_RESOLUTION/2.0f;

        int type = 1;

        if(is_random_z)
            type = 3;

        //if(in_bound(px, py))
        //    vals[(int)py][(int)px] = type;

        vals.push_back({px, py, type, brightness});

    }
}


///remove all references to the map
vector<star_info> standard_scatter()
{
    vector<star_info> vals;

    float brightness_close = 1.5f;
    float brightness_old = 0.7f;
    float brightness_further = 1.2f;

    ///scatter further away from spiral arm
    for(float i=0; i<2.0*M_PI; i+=0.002)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += MAP_RESOLUTION/2.0f;
        y += MAP_RESOLUTION/2.0f;
        nx += MAP_RESOLUTION/2.0f;
        ny += MAP_RESOLUTION/2.0f;

        if(x < 0 || x >= MAP_RESOLUTION || y < 0 || y >= MAP_RESOLUTION)
            continue;
        if(nx < 0 || nx >= MAP_RESOLUTION || ny < 0 || ny >= MAP_RESOLUTION)
            continue;

        float brightness = brightness_further * (randf() + 0.5)/1.5;

        float dist = val;

        different_scatter(x, y, vals, fabs(dist), 4, 0.001, brightness);
        different_scatter(nx, ny, vals, fabs(dist), 4, 0.001, brightness);
    }

    ///scatter old stars
    for(float i=0; i<2.0*M_PI; i+=0.01)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += MAP_RESOLUTION/2.0f;
        y += MAP_RESOLUTION/2.0f;
        nx += MAP_RESOLUTION/2.0f;
        ny += MAP_RESOLUTION/2.0f;

        if(x < 0 || x >= MAP_RESOLUTION || y < 0 || y >= MAP_RESOLUTION)
            continue;
        if(nx < 0 || nx >= MAP_RESOLUTION || ny < 0 || ny >= MAP_RESOLUTION)
            continue;

        float brightness = brightness_old * (randf() + 0.5)/1.5;

        float dist = val;

        ///plot old stars, they have type 2
        different_scatter(x, y, vals, fabs(dist), 8, 0.001, brightness, 2);
        different_scatter(nx, ny, vals, fabs(dist), 8, 0.001, brightness, 2);
    }

    ///scatter.... closer to spiral arm?
    for(float i=0; i<2.0*M_PI; i+=0.002)
    {
        float val = func(i);

        float x = val*cos(i);
        float y = val*sin(i);
        float nx = -val*cos(i);
        float ny = -val*sin(i);

        x += MAP_RESOLUTION/2.0f;
        y += MAP_RESOLUTION/2.0f;
        nx += MAP_RESOLUTION/2.0f;
        ny += MAP_RESOLUTION/2.0f;

        if(x < 0 || x >= MAP_RESOLUTION || y < 0 || y >= MAP_RESOLUTION)
            continue;
        if(nx < 0 || nx >= MAP_RESOLUTION || ny < 0 || ny >= MAP_RESOLUTION)
            continue;

        float brightness = brightness_close * (randf() + 0.5)/1.5;

        float dist = val;

        different_scatter(x, y, vals, fabs(dist), 4, 0.0003, brightness);
        different_scatter(nx, ny, vals, fabs(dist), 4, 0.0003, brightness);
    }

    int mult_factor = 4;

    //random_points_less_edge(vals, 400 * mult_factor, 2.5, true);
    random_points(vals, 800 * mult_factor, 2.5, 1.0, true);
    random_points(vals, 800 * mult_factor, 1.5, 1.0, true);
    random_points(vals, 800 * mult_factor, 0.5, 1.0, true);
    random_points_less_edge(vals, 400 * mult_factor, 2.5, 0.8);
    random_points_less_edge(vals, 2000 * mult_factor, 1.5, 1.0);
    random_points_less_edge(vals, 1000 * mult_factor, 1.75, 1.1);
    random_points_less_edge(vals, 2000 * mult_factor, 1.5, 1.2);
    random_points_less_edge(vals, 2000 * mult_factor, 0.15, 1.7);
    random_points_less_edge(vals, 5000 * mult_factor, 0.35, 1.45);
    random_points_less_edge(vals, 2000 * mult_factor, 0.45, 1.4);
    random_points_less_edge(vals, 500 * mult_factor, 0.45, 1.5, false, 2); ///scatter some oldes around

    return vals;
}

float evaluate_func(float x)
{
    if(x > 0)
        x = -x;

    x = x*60.0f;

    return pow((tanh(x/2 + 2) + 1)/2, 0.3)*1.5;
}


point_cloud construct_starmap(vector<star_info>& vals)
{
    vector<cl_float4> positions;
    vector<cl_uint> colours;

    const float bluepercent = 0.6;

    const float bluemod = 0.9;
    const float yellowmod = 0.8;
    const float redmod = 0.5;

    for(int i=0; i<vals.size(); i++)
    {
        int my_val = vals[i].type;

        float x = vals[i].x;
        float y = vals[i].y;

        float brightness_rand = randf();
        //brightness_rand *= brightness_rand;
        brightness_rand = pow(brightness_rand, 4.f);
        brightness_rand *= 255.f;

        brightness_rand *= vals[i].brightness;

        brightness_rand = clamp(brightness_rand, 0.f, 255.f);


        if(my_val == 1 || my_val == 2 || my_val == 3)
        {
            float rad = RADIUS*1.5;

            float xd = x - MAP_RESOLUTION/2.0f;
            float yd = y - MAP_RESOLUTION/2.0f;

            float dist = sqrt(xd*xd + yd*yd);

            float frac = dist/rad;

            float z = evaluate_func(frac);

            //float gap = z;

            z *= (float)rand()/RAND_MAX;

            if(dist > rad || my_val == 3)
            {
                z = (float)rand()/RAND_MAX;

                if(rand()%2)
                    z = -z;

                z *= 10.0f;
            }

            float randfrac = ((float)rand()/RAND_MAX) - 0.5;

            z += (randfrac)/1.1;

            if(rand()%2) ///hitler
                z = -z;

            cl_float4 col = {0};

            float brightnessmod = (0.8*(float)rand()/RAND_MAX) - 0.3;

            if(my_val == 1 || my_val == 3)
            {
                col = {255,200,150};

                float yrand = yellowmod + brightnessmod;

                if(yrand > 1)
                    yrand = 1;

                col.x *= yrand;
                col.y *= yrand;
                col.z *= yrand;

                if((float)rand()/RAND_MAX < bluepercent)
                {
                    col = {150, 200, 255};

                    float brand = bluemod + brightnessmod;

                    if(brand > 1)
                        brand = 1;

                    col.x *= brand;
                    col.y *= brand;
                    col.z *= brand;
                }
            }
            else if(my_val == 2)
            {
                col = {255, 30, 30};

                float rrand = redmod + brightnessmod;

                if(rrand > 1)
                    rrand = 1;

                col.x *= rrand;
                col.y *= rrand;
                col.z *= rrand;
            }

            col.x = min((int)col.x, 255);
            col.y = min((int)col.y, 255);
            col.z = min((int)col.z, 255);


            cl_float4 pos;
            pos.x = xd*10 + (((float)rand()/RAND_MAX) - 0.5) * 8;
            pos.z = yd*10 + (((float)rand()/RAND_MAX) - 0.5) * 8;
            pos.y = z*100 + (((float)rand()/RAND_MAX) - 0.5) * 8;
            pos.w = 0;

            cl_uint colour = ((int)col.x << 24 | (int)col.y << 16 | (int)col.z << 8) | (uint8_t)brightness_rand;

            positions.push_back(pos);
            colours.push_back(colour);

        }
    }

    float scale_factor = 10.0f;
    //float scale_factor = 10.0f;

    for(auto& i : positions)
    {
        i.x *= scale_factor;
        i.y *= scale_factor;
        i.z *= scale_factor;
    }

    return {positions, colours};

    //return svec;
}

point_cloud get_starmap(int rand_val)
{
    srand(rand_val);

    vector<star_info> vals = standard_scatter();
    point_cloud stars = construct_starmap(vals);

    //shitty_dealloc(vals);

    return stars;
}

#ifdef GALAXY_TEST
int main()
{
    sf::RenderWindow window;
    window.create(sf::VideoMode(800, 600), "lele");


    sf::Image img;
    img.create(800, 600, sf::Color(0, 0, 0));

    point_cloud stars = get_starmap(1);

    sf::Event event;

    cl_float4 cam = {0,0,0,0};
    cl_float4 pos = {0,0,0,0};

    sf::Keyboard key;

    while(window.isOpen())
    {
        if(window.pollEvent(event))
        {
            if(event.type == sf::Event::Closed)
                window.close();
        }

        if(key.isKeyPressed(sf::Keyboard::Escape))
            window.close();


        //for(auto& i : stars)
        for(int i=0; i<stars.position.size(); i++)
        {
            /*if(i.type == 1)
                col = sf::Color(255, 200, 150);
            if(i.type == 2)
                col = sf::Color(255, 0, 0);*/

            auto pos = stars.position[i];
            auto col = stars.rgb_colour[i];

            pos = div(pos, 100.f);

            int r = col >> 24;
            int g = col >> 16;
            int b = col >> 8;


            float x, y;

            x = pos.x + 400;
            y = pos.y + 300;

            if(x >= 800 || y >= 600 || x < 0 || y < 0)
            {
                continue;
            }

            sf::Color colour(r, g, b);

            img.setPixel(x, y, colour);
        }


        sf::Texture tex;
        tex.loadFromImage(img);

        sf::Sprite spr(tex);

        window.draw(spr);
        window.display();
    }

    return 0;
}
#endif

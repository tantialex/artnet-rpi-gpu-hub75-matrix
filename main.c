/**
 * This is an example program using the rpi-gpu-hub75 library.
 * To compile:
 * gcc -O3 -Wall -lrpihub75_gpu main.c -o example
 * ./example
 * # this should display command line options
 * # example for 4 64x64 panels (128x128 pixels) 2 chains connected to 2 ports, 48 pwm color bits, 192 brightness, 2.2 gamma
 * # render the cartoon.glsl shader at 120 fps
 * ./example -w 128 -h 128 -p 2 -c 2 -d 48 -b 192 -g 2.2 -f 120 -s shaders/cartoon.glsl
 *
 */

#include <pthread.h>
#include <rpihub75/rpihub75.h>
#include <rpihub75/util.h>
#include <rpihub75/gpu.h>
#include <rpihub75/pixels.h>

unsigned int ri(unsigned int max) {
	return rand() % max;
}

// our CPU rendering implementation, see gpu.c for shader rendering details
void* render_cpu(void *arg) {
    // get the current scene info
    scene_info *scene = (scene_info*)arg;
    const int buffer_sz     = scene->width * scene->height * scene->stride;
    uint32_t frame          = 0;
    uint8_t *img = malloc(scene->width * scene->height * scene->stride);

    uint16_t w = scene->width;
    uint16_t h = scene->height;
    memset(img, 0, scene->width * scene->height * scene->stride);

    printf("RENDER CPU, stride: %d!\n", scene->stride);
    //usleep(1);
    printf("BEGIN..\n");
    // loop forever on this thread

/*
    for (int x=0; x<buffer_sz; x++) {
        scene->image[x] = ri(128);//(uint8_t)(scene->image[x] * 0.99f);
    }
    */

    for (int y=0; y<scene->height; y++) {
        for (int x=0; x<scene->width; x++) {
            //RGB c = {ri(128), ri(128), ri(128)};
            //RGB c = {0, 0, 192};
            //hub_pixel(scene, x, y, c);
            int o= ((y*scene->width)+x * scene->stride);
            img[o] = 0;
            img[o+1] = 0;
            img[o+2] = 0;
    //printf("o: %d\n", o);
        }
    //file_put_contents("/tmp/out.rgb", img, scene->width*scene->height *scene->stride);
    //die("exit\n");
    }


    for(;;) {
        // darken every pixel in the image for each byte of R,G,B data
        /*
        for (int y=0; y<scene->height; y++) {
            uint32_t yoff = y*scene->width*scene->stride;
            for (int x=0; x<scene->width; x++) {
                uint32_t xoff = x * scene->stride;
                uint32_t src = yoff + ((xoff+3) % scene->width);
                uint32_t dst = yoff + ((xoff) % scene->width);
                scene->image[dst] = scene->image[src];
                scene->image[dst+1] = scene->image[src+1];
                scene->image[dst+2] = scene->image[src+2];
            }
        }
        */

    if (1) { //frame % 1 == 1) {
        for (int i=0; i<scene->height*scene->width*scene->stride; i++) {
            scene->image[i] = (uint8_t)scene->image[i] * 0.92f;
        }
    }


// XXX fix
        uint16_t x1 = ri(255);
        uint16_t x2 = ri(255);
        uint16_t y1 = ri(h-8);
        uint16_t y2 = y1 + ri(h-y1);
        RGB color = {ri(250), ri(250), ri(250)};

        // draw random square - this function is provided by rpihub75 library
        //draw_square(image, scene->width, scene->height, scene->stride);

        //printf("fill %dx%d - %dx%d\n", x1, y1, x2, y2);
        hub_fill(scene, x1, y1, x2, y2, color); 
        //hub_fill(scene, 1, 1, 2, 2, color); 
        //hub_pixel(scene, 0, 1, color);
        /*
        int off = ((5*scene->width)+15)* scene->stride;
        img[off] = 90;
        img[off+1] = 50;
        img[off+2] = 250;
        */

        // render the RGB data to the active PWM buffers. sleep delay the frame to sync with scene->fps
        //PRE_TIME
        scene->pwm_mapper(scene, NULL, 1);
        //POST_TIME
        usleep(1000000 / scene->fps);
        //usleep(1000);
        

        calculate_fps(0);
        frame++;
        //if (frame % 100 == 1) {
        //printf("frame: %d\n", frame);
        //}
    }
}


int main(int argc, char **argv)
{
    srand(time(NULL));
    // parse command line options to define the scene
    // use -h for help, see this function in util.c for more information on command line parsing
    scene_info *scene = default_scene(argc, argv);

    // ensure that the scene is valid
    check_scene(scene);
    
    // create another thread to run the frame drawing function (GPU or CPU)
    pthread_t update_thread;
    // use the gpu shader renderer if we have one, else use the cpu renderer above
    if (scene->shader_file == NULL) {
        pthread_create(&update_thread, NULL, render_cpu, scene);
    } else {
        scene->stride = 4;
        pthread_create(&update_thread, NULL, render_shader, scene);
    }

    // this function will never return. make sure you have already forked your drawing thread 
    // before calling this function
    render_forever(scene);
}

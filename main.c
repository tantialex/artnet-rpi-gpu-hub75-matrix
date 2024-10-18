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

// our CPU rendering implementation, see gpu.c for shader rendering details
void* render_cpu(void *arg) {
    // get the current scene info
    const scene_info *scene = (scene_info*)arg;
    const int buffer_sz     = scene->width * scene->height * scene->stride;
    uint8_t *image          = (uint8_t*)malloc(buffer_sz);
    uint32_t frame          = 0;

    // loop forever on this thread
    for(;;) {
        // darken every pixel in the image for each byte of R,G,B data
        for (int x=0; x<buffer_sz; x++) {
            image[x] = (uint8_t)(image[x] * 0.99f);
        }

        // draw random square - this function is provided by rpihub75 library
        draw_square(image, scene->width, scene->height, scene->stride);

        // render the RGB data to the active PWM buffers. sleep delay the frame to sync with scene->fps
        scene->pwm_mapper(image, scene, TRUE);

        frame++;
        printf("frame: %d\n", frame);
    }
}


int main(int argc, char **argv)
{
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
        pthread_create(&update_thread, NULL, render_shader, scene);
    }

    // this function will never return. make sure you have already forked your drawing thread 
    // before calling this function
    render_forever(scene);
}

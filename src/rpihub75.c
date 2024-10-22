/**
 * https://www.i-programmer.info/programming/148-hardware/16887-raspberry-pi-iot-in-c-pi-5-memory-mapped-gpio.html
 * This code was made possible by the work of Harry Fairhead to describe the RPI5 GPIO interface.
 * As Raspberry Pi5 support increases, this code will be updated to reflect the latest GPIO interface.
 * 
 * After Linux kernel 6.12 goes into Raspberry pi mainline, you should compile the kernel with
 * PREEMPT_RT patch to get the most stable performance out of the GPIO interface.
 * 
 * This code does not require root privileges and is quite stable even under system load.
 * 
 * This is about 80 hours of work to deconstruct the HUB75 protocol and the RPI5 GPIO interface
 * as well as build the PWM modulation, abstractions, GPU shader renderer and debug. 
 * 
 * You are welcome to use and adapt this code for your own projects.
 * If you use this code, please provide a back-link to the github repo, drop a star and give me a shout out.
 * 
 * Happy coding...
 * 
 * @file gpio.c
 * @author Cory A Marsh (coryamarsh@gmail.com)
 * @brief 
 * @version 0.33
 * @date 2024-10-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <sched.h>
#include <pthread.h>
#include <stdatomic.h>

#include "rpihub75.h"
#include "util.h"


/**
 * @brief calculate an address line pin mask for row y
 * 
 * @param y the panel row number to calculate the mask for
 * @return uint32_t the bitmask for the address lines at row y
 */
uint32_t row_to_address(const int y) {

    // if they pass in image y not panel y, convert to panel y
    uint16_t row = (y -1) % PANEL_HEIGHT;
    uint32_t bitmask = 0;

    // Map each bit from the input to the corresponding bit position in the bitmask
    if (row & (1 << 0)) bitmask |= (1 << ADDRESS_A);  // Map bit 0
    if (row & (1 << 1)) bitmask |= (1 << ADDRESS_B);  // Map bit 1
    if (row & (1 << 2)) bitmask |= (1 << ADDRESS_C);  // Map bit 2
    if (row & (1 << 3)) bitmask |= (1 << ADDRESS_D);  // Map bit 3
    if (row & (1 << 4)) bitmask |= (1 << ADDRESS_E);  // Map bit 4


    //printf("row %d = %x\n", row, bitmask);
    return bitmask;
}




/**
 * @brief verify that the scene configuration is valid
 * @param scene 
 */
void check_scene(const scene_info *scene) {
    //uint16_t max_fps = 9600 / scene->bit_depth / (scene->panel_width / 16);
    uint16_t max_fps = 19200 / scene->bit_depth / (scene->panel_width / 16);
    (void)max_fps;
    /*
    if (scene->fps > max_fps) {
        fprintf(stderr, "FPS too high for current configuration. max fps is: %d\n", max_fps);
        exit(EXIT_FAILURE);
    }
    */
    if (scene->num_ports > 3) {
        die("Only 3 port supported at this time\n");
    }
    if (scene->num_ports < 1) {
        die("Require at last 1 port\n");
    }
    if (scene->num_chains < 1) {
        die("Require at last 1 panel per chain\n");
    }
    if (scene->num_chains > 16) {
        die("max 16 panels supported on each chain\n");
    }
    if (scene->pwm_mapper == NULL) {
        die("A pwm mapping function is required\n");
    }
    if (scene->stride != 3 && scene->stride != 4) { 
        die("Only 3 or 4 byte stride supported\n");
    }
    if (scene->pwm_signalA == NULL) {
        die("No pwm signal buffer defined\n");
    }
    if (scene->image == NULL) {
        die("No RGB image buffer defined\n");
    }
    if (scene->bit_depth < 8 || scene->bit_depth > 64) {
        die("Only 8-64 bit depth supported\n");
    }
    if (scene->motion_blur_frames > 32) {
        die("Max motion blur frames is 32\n");
    }

    if (scene->bit_depth % BIT_DEPTH_ALIGNMENT != 0) {
        die("requested bit_depth %d, but %d is not aligned to %d bytes\n"
            "To use this bit depth, you must #define BIT_DEPTH_ALIGNMENT to the\n"
            "least common denominator of %d\n", 
            scene->bit_depth, scene->bit_depth, BIT_DEPTH_ALIGNMENT);
    }
}


/**
 * @brief you can cause render_forever to exit by updating the value of do_hub65_render pointer
 * EG:
 * 
 * 
 */
__attribute__((noreturn))
void render_forever(const scene_info *scene) {

    srand(time(NULL));
    // map the gpio address to we can control the GPIO pins
    uint32_t *PERIBase = map_gpio(0); // for root on pi5 (/dev/mem, offset is 0xD0000)
    // offset to the RIO registers (required for #define register access. 
    // TODO: this needs to be improved and #define to RIOBase removed)
    uint32_t *RIOBase  = PERIBase + RIO_OFFSET;

    // configure the pins we need for pulldown, 8ma and output
    configure_gpio(PERIBase);

    // create the OE jitter mask to control screen brightness
    uint32_t *jitter_mask = create_jitter_mask(JITTER_SIZE, scene->brightness);
    if (scene->jitter_brightness == false) {
        for (int i=0; i<JITTER_SIZE; i++) {
            jitter_mask[i] = 0x0;
        }
    }
 
    // render pwm image data to the GPIO pins forever...
    uint16_t toggle_pwm = 0;
    const uint8_t  half_height __attribute__((aligned(16))) = scene->panel_height / 2;
    const uint16_t width __attribute__((aligned(16))) = scene->width;
    const uint8_t  bit_depth __attribute__((aligned(BIT_DEPTH_ALIGNMENT))) = scene->bit_depth;
    ASSERT(width % 16 == 0);
    ASSERT(half_height % 16 == 0);
    ASSERT(bit_depth % BIT_DEPTH_ALIGNMENT == 0);
    while(scene->do_render) {
        const uint32_t *__attribute__((aligned(16))) pwm_signal = scene->pwm_signalA;
        const uint8_t bit_depth __attribute__((aligned(BIT_DEPTH_ALIGNMENT))) = scene->bit_depth;

        // iterate over the bit plane
        for (uint8_t pwm=0; pwm<bit_depth; pwm++) {
            // for the current bit plane, render the entire frame
            for (uint16_t y=0; y<half_height; y++) {
                asm volatile ("" : : : "memory");  // Prevents optimization

                // compute the line address for this row
                const uint32_t address_mask = row_to_address(y);
                uint32_t offset = ((y * scene->width + (0)) * bit_depth) + pwm;
                for (uint16_t x=0; x<width; x++) {
                    asm volatile ("" : : : "memory");  // Prevents optimization
                    // set all bits for clock cycle at once
                    //rio->Out = pwm_signal[offset] | address_mask | PIN_CLK | jitter_mask[toggle_pwm];
                    // toggle clock pin low
                    //rioCLR->Out = PIN_CLK;

                    // set all bits for clock cycle at once, toggle clock low
                    rio->Out = pwm_signal[offset] | address_mask | jitter_mask[toggle_pwm];
                    // advance the global OE jitter mask 1 frame
                    toggle_pwm = (toggle_pwm + 1) % JITTER_SIZE;
                    // toggle clock pin high
                    rioSET->Out = PIN_CLK;

                    // advance to the next bit in the pwm signal
                    offset += bit_depth;
                }
                // make sure enable pin is high (display off) while we are latching data
                rio->Out = PIN_OE;
                // latch the data for the entire row
                rioSET->Out = PIN_LATCH;
                rioCLR->Out = PIN_LATCH;
            }
        }
    }
}
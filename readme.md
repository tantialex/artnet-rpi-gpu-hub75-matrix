This library implements HUB75 protocol on the rpi5
==================================================

This is based on the work done by hzeller adding HUB75 support to RPI (www.github.com/hzeller/rpi-rgb-led-matrix)
as well as the work of Harry Fairhead documenting the peripheral address space for rpi5(https://www.i-programmer.info/programming/148-hardware/16887-raspberry-pi-iot-in-c-pi-5-memory-mapped-gpio.html)

BitBang HUB75 data at 20Mhz. Supports 9600Hz refresh rate on single 64x64 panel. Supports up to 3 ports per clock
cycle (18 pixels worth of on/off data per clock cycle). Supports updating frame data at any moment so frame rates
of >120Hz are easily possible. Support for up to 64bits of pwm data (1/64 pwm cycle for 64 different color levels
for each RGB value). No hardware clocks are required for operation so you can run the code with only group gpio
privileges. Operation is mostly flicker free, however, you should see the improved response by running with nice -n -20
and running the real-time PREEMPT_RT patch on the kernel (6.6) as of this writing. PREEMPT_RT is mainline in 6.12
so hopefully no patches are required on the next raspbian release!

I have included GPU support using Linux's Generic Buffer Manager (gbm), GLESv2 and EGL. This means you can use 
OpenGL fragment shaders to render PWM data to the hub75 panel. Several shadertoy shaders are included.

This implementation only supports rpi5 at the moment. It should be simple to add support for other PIs as only
the memory-mapped peripheral address for the GPIO pins is required. Preliminary GPIO peripheral offsets are in
rpihub75.h. There is a #ifdef PI3, PI4 and it defaults to PI5. If you are inclined, please test on an earlier PI
and send a PR with the correct offsets for ZERO, PI3 and PI4.


Please read HZeller's excellent write-up on wiring the PI to the HUB75 display.  I highly recommend using one of his
active 3 port boards to ensure proper level translation and to map the address lines, OE and clock pins to all 3 boards

Hub75 Operation
---------------
All documentation reefers to 64x64 panels. Your Mileage May Vary.
Pins: r1,r2,g1,g2,b1,b2, this is the color data. each LED is actually 3 leds (red, green and blue). The LED can be
on or off. We will be pulsing them on/off very quickly to achieve the illusion of different color values. Color data
is sent 2 pixels at a time beginning on column 0. r1,g1,b1 is the pixel in the upper 32 rows, r2,g2,b2 is the pixel
in the lower 32 rows.

A,B,C,D,E are the address lines. These 5 pins represent the row address. 2^5 = 32 so depending on which bitmask is
set on the address lines, the 2 corresponding led rows will be addressed for shifting in data. Data is shifted in on
the falling edge of CLK. so after setting the address line, we set pixel value for column 0 along with clock, and then
we pull the clock low. That is pixel 0. We now shift in the next pixel and so on 64 times. If we have multiple panels we
simply continue shifting in data (in 64 column chunks) for as many panels as we have.

To actually update the panel, we must bring OE (output enable) line high (to disable to display) and toggle the latch
pin. data for one row is now latched. we advance the address row lines drop the enable pin low (turn the display on)
and begin the process again.


Overview
--------
The library bit bangs the data out to the HUB75 panel at a steady 20Mhz. This is significantly faster on my scope than
hzeller's implementation by up to 10x. The software forks a thread that pulls from the pwm data and continuously pulses 
the rgb pins and the clock line.  After each row, Output Enable pin is driven high and the data is latched and the next
row is advanced. After 32 rows (or 1/2 panel height) are written to all 3 ports, the pwm buffer is advanced and the 
update begins again for the next "bit plane".

Since we are clocking in 2 rows of RGB data per clock cycle (R1, R2, G1, G2, B1, B2) at 20Mhz it takes 2048 clock cycles
to shift in data for 1 64x64 panel. This translates to a single 64x64 panel refresh rate of 9700Hz and allows us to chain
4 panels together per port at >2400Hz. 

Rather than call "SetPixel", you draw directly to a 24bpp or 32bpp buffer and then call this library's function
map_byte_image_to_pwm() to translate the 24bpp RGB buffer to the pwm signal. Example:

```c
scene_info *scene = default_scene(argc, argv);
// example scene->stride is 3 for 24bpp (3 bytes per pixel)
uint8_t *imageRGB = (uint8_t*)malloc(scene->width * scene->height * scene->stride); 

int x = 32;
int y = 32;
uint8_t red = 255; 
uint8_t green = 128; 
uint8_t blue = 64; 

imageRGB[((y*scene->width) +x *scene->stride)] = red;
imageRGB[((y*scene->width) +x *scene->stride)+1] = green;
imageRGB[((y*scene->width) +x *scene->stride)+2] = blue;
```

for convenience, you can use the set_pixel24 and set_pixel32 helpers to write image data similar to hzeller's library.
these functions are inline versions of the above code.



```c
// need to call default_scene() first to correctly setup the global scene->width
// extern int global_width;
scene_info *scene = default_scene(argc, argv);
// write to a 24bpp RGB buffer (3 bytes per pixel)
set_pixel24(imageRGB, 32, 32, 255, 128, 0);

// write to a 32bpp RGB buffer (4 bytes per pixel, 4th byte alpha channel is ignored)
set_pixel32(imageRGBA, 32, 32, 255, 128, 0);
```

32bpp RGBA buffer support is useful when pulling data from OpenGL which will output 32bpp RGBA data.



Users can update either 24bpp RGB or 32bpp RGBA frame buffers directly and then call map_byte_image_to_pwm() after rendering
a new frame. Calling this method will translate the RGB data to pwm bit data. pwm data is organized as a multi dimensional
array of uint32_t data. Each uint32_t stores a bitmask for the r1,r2,g1,g2,b1,b2 pins for the current pixel's bit-plane. There
is no need to call any other functions as the "render_forever()" code pulls directly from this buffer.


RGB to PWM Mapping
------------------

Data is indexed as follows where pwm is the current bit plane index (0 - bit_depth):

int offset = ((y * scene->width + (x)) * scene->bit_depth) + pwm;

using a linear mapping for RGB (255, 128, 0), the pwm data for a single pixel would map to:
r: 1,1,1,1,1,1,1,1,1,1,1...
g: 1,0,1,0,1,0,1,0,1,0,1...
b: 0,0,0,0,0,0,0,0,0,0,0...

these values would be precomputed after every frame and toggled for each display update (9600-2400Hz)


each bit plane (that is a uint32_t with all of the pin toggles for all 3 output ports for a particular pixel on a single 
bit plane, there are bit_depth number of bit planes per image) is updated atomically in a single write. This means there
is no need for double buffering to achieve flicker-free display. Simply call map_byte_image_to_pwm with your new image
buffer as often as you like. The data will be overwritten and the new PWM data will be updated immediately. This allows you
to draw to the display at up to 9600Hz (depending on the number of chained displays) however frame rates of about 120fps seem 
to produce excellent results and higher frame rates have diminishing returns after that.

Because we are have a 9600-2400Hz refresh rate we are able to use up to 64bit PWM cycles. That means that each RGB value
can have 64 levels of brightness or 64*64*64 = 262144 colors. In practice though, values at the lower end (more bits off
than on) have a much more perceptible effect on brightness than values at the higher end (more on than off). This is because
the human eye is much more sensitive to brightness changes in darker levels than at brighter levels. To correct this I have
added gamma correction. See color calibration further in this document for details.


brightness is controlled via a 9K "jitter mask". 9K of random bytes are generated and if each random value is > brightness
level (uint8_t) the OE pin is toggled for the mask. when OE is toggled high, the display is toggled off. By applying this 
mask for every pixel, we are able to output our normal pwm color data and toggle the brightness value on or off randomly
averaging out to the current brightness level. This provides for very fine tuned brightness control (255 levels) while
maintaining excellent color balance.

Alternatively, you can encode brightness data directly into the PWM data, however, this yields very poor results for low
brightness levels even when using 64 bits of pwm data. this is controlled via the scene_info->jitter_brightness boolean.


The mapping from 24bpp (or 32bpp) RGB data to pwm data is very optimized. It uses 128-bit SIMD vectors for the innermost
loop. I have attempted to remove the (!!( operator to no avail. If you can improve the bit operations in this loop, this
is 90% of the program time. mask_lookup is 1ULL << j. pwm_signal[offset] is the current pixel start of the pwm bit plane.
pwm_clear_mask is an inverse bit mask for the RGB pins of the current port. port[0] - port[6] are the rgb pins 1-6.
r1_pwm is the linear 8-bit rgb value to 64-bit pwm dat a lookup table for red, green, blue. etc.


```c
for (int j=0; j<bit_depth; j++) {
        // mask off just this bit plane's data
        uint64_t mask = mask_lookup[j];

        // clear just the bits on this port for this bit plane so we don't accidentally clear other rgb ports
        // Set the bits if the pwm value is set for this bit plane using bitmask instead of ternary
        // the clear_mask sets all bits for other ports to 1 so that we don't interfere with the other ports
        pwm_signal[offset] = (pwm_signal[offset] & pwm_clear_mask) |
            (port[0] & (uint64_t)-(!!(r1_pwm & mask))) |
            (port[1] & (uint64_t)-(!!(r2_pwm & mask))) |
            (port[2] & (uint64_t)-(!!(g1_pwm & mask))) |
            (port[3] & (uint64_t)-(!!(g2_pwm & mask))) |
            (port[4] & (uint64_t)-(!!(b1_pwm & mask))) |
            (port[5] & (uint64_t)-(!!(b2_pwm & mask)));
        offset++;
    }
```


GPU Support
-----------
To add GPU shader support you will need to install glesv2, gbm and mesagl.
sudo apt-get install libgles2-mesa-dev libgbm-dev libegl1-mesa-dev

support for single buffer shadertoy shaders is already added so just pass your shader via the -s command line
parameter. This will set the path to the shader in the scene_info->shader string. render_shader() in gpu.c
will look for a shader on the filesystem at path scene_info->shader and attempt to compile it. It will update
glUniforms iTime and iResolution like shadertoy, however no support for additional buffers or textures has
been added as of yet. Send a PR if you are inclined.

After rendering the shader, the frame buffer is read using glReadPixels() and pwm_mapped the same as the
CPU renderer. the render_shader loop does not return. It will attempt to usleep until scene->fps is matched.
If the GPU can not keep up with the current fps, no sleep is performed.



Compiling and Installing
------------------------
to build and install library and header files to /usr/local run:


```bash
# compile version without GPU support
make lib
# compile version with GPU support
make libgpu
# compile both versions
make 
# install headers and libraries in /usr/local
sudo make install

# to compile your app without GPU support:
gcc -lrpihub75 myapp.c -o myapp 

# to compile your app with GPU support:
gcc -lrpihub75_gpu myapp.c -o myapp 

# print command line configuration help
./myapp 

# run glsl shader app for 1 64x64 panel on port0, 64 bits pwm depth, gamma 2.2, 50% brightness
./myapp -p 1 -c 1 -h 64 -w 64 -d 64 -g 2.2 -b 128 -s shaders/cartoon.glsl

```

Example Program
---------------

```c
// see main.c for this example
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

    // loop forever on this thread
    for(;;) {
        // darken every pixel in the image
        for (int x=0; x<buffer_sz; x++) {
            image[x] = (uint8_t)(image[x] * 0.99f);
        }

        // draw random square - this function is provided by rpihub75 library
        draw_square(image, scene->width, scene->height, scene->stride);

        // render the RGB data to the active PWM buffers. sleep delay the frame to sync with scene->fps
        scene->pwm_mapper(image, scene, TRUE);
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

    // this function will never return
    render_forever(scene);
}
```


Command Line Arguments
----------------------
You can configure your setup for your application from the command line if you so choose by adding the call: 
```c
    scene_info *scene = default_scene(argc, argv);
```

This will parse the following command line parameters and setup your scene_info configuration for you.
If you prefer you can also hard code this configuration or load it from a configuration file. This structure
is required to call render_forever() and the scene->pwm_mapper() function pointer points to the current
pwm_mapping function for the scene. (see func_pwm_mapper_t)


```txt
 Usage: ./example
     -s <shader file>  GPU fragment shader file to render
     -x <width>        image width              (16-384)
     -y <height>       image height             (16-384)
     -w <width>        panel width              (16/32/64)
     -h <height>       panel height             (16/32/64)
     -f <fps>          frames per second        (1-255)
     -p <num ports>    number of ports          (1-3)
     -c <num chains>   number of chains         (1-16)
     -g <gamma>        gamma correction         (1.0-2.8)
     -d <bit depth>    bit depth                (2-32)
     -b <brightness>   overall brightness level (0-255)
     -j                disable jitter, adjust PWM brightness instead
     -l <frames>       motion blur frames       (0-32)
     -v                vertical mirror image
     -m                mirror output image
     -i <mapper>       image mapper (u, image mapper not completed yet)
     -t <tone_mapper>  (aces, reinhard, habble)
     -z                run LED calibration script
     -n                display data from UDP server on port 22222
     -h                this help
```



Odds and Ends
-------------

I have considered adding image dithering. Since we are going from 8-bit data (24bpp) down to 5 or 6-bit data (32 - 64 bit
pwm values) we are losing 2-3 bits of data per pixel. What we lose in temporal data (value) we can reintroduce to the image
spatially. Those 2-3 bits of information can be added to the neighboring pixels using ordered dithering or floyd steinberg 
dithering by slightly increasing or decreasing the RGB values of the neighboring pixels based on this loss of information.
There is some code to achieve this but I have not had the results I would like to see so this is still a work in progress.
If this is something you are interested in, drop me a line, send me a link to relevant information implementations or send
a PR.


Compiler Flags
--------------
* be sure to add -O3 to gcc when compiling your source.
* -ffast-math reduces some precision but improves performance for some cases.
* -mtune=native will compile for your CPU optomizations.
* -Wdouble-promotion can help identify a lot of places where you are actually using double precision (64 bit) not single precision (float)
   obviously 64bit floats are at least 2x slower than single precision.
* -fopt-info-vec will show you loops the compiler was able to vectorize for you (SIMD, AVX, SSE, ETC)



investigation:
blue noise dithering

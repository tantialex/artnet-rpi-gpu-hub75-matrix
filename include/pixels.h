#include <stdint.h>
#include "rpihub75.h"

#ifndef _HUB75_PIXELS_H
#define _HUB75_PIXELS_H 1

// ACES tone mapping constants
#define ACES_A 2.51f
#define ACES_B 0.03f
#define ACES_C 2.43f
#define ACES_D 0.59f
#define ACES_E 0.14f

// Constants for Uncharted 2 tone mapping
#define UNCHART_A 0.15f
#define UNCHART_B 0.50f
#define UNCHART_C 0.10f
#define UNCHART_D 0.20f
#define UNCHART_E 0.02f
#define UNCHART_F 0.30f
#define UNCHART_W 11.2f  // White point (adjust as needed)


/**
 * @brief function definition to function that maps RGB image data
 * to BCM data for shifting out to GPIO
 */
typedef void (*update_bcm_signal_fn)(
    const scene_info *scene,
    const void *bits,  // Use void* to handle both uint32_t* and uint64_t*
    uint32_t *bcm_signal,
    uint8_t *image);
    // XXX FIX
    //const uint8_t *image);

/**
 * @brief update_bcm_signal_fn implementation for up to 64 bit BCM data
 * 
 * @param scene  pointer to current scene info
 * @param void_bits RGB to BCM index data. Red 0-255, Green 256-511, Blue 512 - 768
 * @param bcm_signal the ouput buffer to write BCM data to
 * @param image the input RGB or RGBA image to render
 * @param offset offset into the bcm buffer
 */
void update_bcm_signal_64(
    const scene_info *scene,
    const void *__restrict__ void_bits,
    uint32_t *__restrict__ bcm_signal,
    uint8_t *images);
    // XXX fix
    //const uint8_t *__restrict__ image,
    //const uint32_t offset);


/**
 * @brief update_bcm_signal_fn implementation for up to 32 bit BCM data
 * 
 * @param scene 
 * @param void_bits 
 * @param bcm_signal 
 * @param image 
 * @param offset 
 */
void update_bcm_signal_32(
    const scene_info *scene,
    const void *__restrict__ void_bits,
    uint32_t *__restrict__ bcm_signal,
    const uint8_t *__restrict__ image);



/**
 * @brief this function takes the image data and maps it to the bcm signal.
 * 
 * if scene->tone_mapper is updated, new bcm bit masks will be created.
 * 
 * @param scene the scene information
 * @param image the image to map to the scene bcm data. if NULL scene->image will be used
 * @param do_fps_sync if true, the function will attempt to usleep delay sync to the scene->fps
 */
void map_byte_image_to_bcm(scene_info *scene, const uint8_t *image, const uint8_t do_fps_sync);

/**
 * @brief convert linear RGB to normalized CIE1931 XYZ color space
 * https://en.wikipedia.org/wiki/CIE_1931_color_space
 * 
 * @param r 
 * @param g 
 * @param b 
 * @param X 
 * @param Y 
 * @param Z 
 */
void linear_rgb_to_cie1931(const uint8_t r, const uint8_t g, const uint8_t b, float *X, float *Y, float *Z);

/**
 * @brief linear interpolate between two floats
 * 
 * @param x value A
 * @param y value B
 * @param a Normal 0-1 interpolation amount
 * @return float 
 */
__attribute__((pure))
float mixf(const float x, const float y, const Normal a);

/**
 * @brief  clamp a value between >= lower and <= upper
 * 
 * @param x value to clamp
 * @param lower  lower bound inclusive
 * @param upper  upper bound inclusive
 * @return float 
 */
__attribute__((pure))
float clampf(const float x, const float lower, const float upper);

/**
 * @brief normalize a uint8_t (0-255) to a float (0-1)
 * 
 * @param in byte to normalize
 * @return Normal a floating point value between 0-1
 */
__attribute__((pure))
Normal normalize8(const uint8_t in);

/**
 * @brief take a normalized RGB value and return luminance as a Normal
 * 
 * @param in value to determine luminance for
 * @return Normal 
 */
__attribute__((pure))
Normal luminance(const RGBF *__restrict__ in);

/**
 * @brief adjust the contrast and saturation of an RGBF pixel
 * 
 * @param in this RGBF value will be adjusted in place. no new RGBF value is returned
 * @param contrast - contrast value 0-1
 * @param saturation - saturation value 0-1
 */
void adjust_contrast_saturation_inplace(RGBF *__restrict__ in, const Normal contrast, const Normal saturation);


/**
 * @brief  perform gamma correction on a single byte value (0-255)
 * 
 * @param x - value to gamma correct
 * @param gamma  - gamma correction factor, 1.0 - 2.4 is typical
 * @return uint8_t  - the gamma corrected value
 */
__attribute__((pure))
uint8_t byte_gamma_correct(const uint8_t x, const float gamma);


/**
 * @brief  perform gamma correction on a normalized value. 
 * this is faster than byte_gamma_correct which has to normalize the values then call this function
 * 
 * 
 * @param x - value to gamma correct
 * @param gamma  - gamma correction factor, 1.0 - 2.4 is typical
 * @return Normal  - the gamma corrected value
 */
__attribute__((pure))
Normal normal_gamma_correct(const Normal x, const float gamma);

/**
 * @brief  perform hable uncharted 2 tone mapping
 * @param color  - the color to tone map 
 * @return Normal  - the gamma corrected value
 */
__attribute__((pure))
Normal hable_tone_map(const Normal color);

/**
 * @brief  perform reinhard tone mapping
 * @param color  - the color to tone map 
 * @return Normal  - the gamma corrected value
 */
__attribute__((pure))
Normal reinhard_tone_map(const Normal color);

/**
 * @brief  perform aces tone mapping
 * @param color  - the color to tone map 
 * @return Normal  - the gamma corrected value
 */
__attribute__((pure))
Normal aces_tone_map(const Normal color);

/**
 * @brief  perform aces tone mapping on RGB values
 * @param in  - the color to tone map 
 * @return out  - the RGB pixel to store the result
 */
void aces_tone_mapper8(const RGB *__restrict__ in, RGB *__restrict__ out);


/**
 * @brief perform ACES tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void aces_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out);

/**
 * @brief perform hable Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void hable_tone_mapper(const RGB *__restrict__ in, RGB *__restrict__ out);

/**
 * @brief perform hable Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void hable_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out);

/**
 * @brief perform hable Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void reinhard_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out);

/**
 * @brief perform ACES reinhard mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void reinhard_tone_mapper(const RGB *__restrict__ in, RGB *__restrict__ out);

/**
 * @brief an empty tone mapper that does nothing
 * 
 * @param in 
 * @param out 
 */
void copy_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out);


/**
 * @brief create a lookup table for the pwm values for each pixel value
 * applies gamma correction and tone mapping based on the settings passed in scene
 * scene->brightness, scene->gamma, scene->red_linear, scene->green_linear, scene->blue_linear
 * scene->tone_mapper
 * 
 */
__attribute__((cold))
void *tone_map_rgb_bits(const scene_info *scene, int num_bits);



/**
 * @brief draw an unfilled triangle using Bresenham's line drawing algorithm
 * 
 * @param scene 
 * @param x0  p0 x
 * @param y0  p0 y
 * @param x1  p1 x
 * @param y1  p2 y
 * @param x2  p3 x
 * @param y2  p3 y
 * @param color 
 */
void hub_triangle(scene_info *scene, int x0, int y0, int x1, int y1, int x2, int y2, RGB color);


/**
 * @brief draw an unfilled triangle using Xiolin Wu's line drawing algorithm
 * 
 * @param scene 
 * @param x0  p0 x
 * @param y0  p0 y
 * @param x1  p1 x
 * @param y1  p2 y
 * @param x2  p3 x
 * @param y2  p3 y
 * @param color 
 */
void hub_triangle_aa(scene_info *scene, int x0, int y0, int x1, int y1, int x2, int y2, RGB color);

void hub_pixel(scene_info *scene, const int x, const int y, const RGB pixel);
void hub_pixel_alpha(scene_info *scene, const int x, const int y, const RGBA pixel);
void hub_fill(scene_info *scene, uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, RGB color);

#endif


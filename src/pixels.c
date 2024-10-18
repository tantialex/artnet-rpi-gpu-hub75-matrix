#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <string.h>


#include "rpihub75.h"
#include "util.h"
#include "pixels.h"

extern uint16_t global_width;

/**
 * @brief convert linear RGB to normalized CIE1931 XYZ color space
 * @todo: replace with RGB 
 * https://en.wikipedia.org/wiki/CIE_1931_color_space
 * 
 * @param r 
 * @param g 
 * @param b 
 * @param X 
 * @param Y 
 * @param Z 
 */
void linear_rgb_to_cie1931(const uint8_t r, const uint8_t g, const uint8_t b, float *X, float *Y, float *Z) {
    // Define the 24-bit linear RGB to XYZ conversion matrix
	const float RGB_to_XYZ[3][3] = {
	    {0.4124564, 0.3575761, 0.1804375},  // X
	    {0.2126729, 0.7151522, 0.0721750},  // Y
	    {0.0193339, 0.1191920, 0.9503041}   // Z
	};


    // Normalize the RGB values to the range [0, 1]
	const Normal norm_r = normalize8(r);
	const Normal norm_g = normalize8(g);
	const Normal norm_b = normalize8(b);

    // Apply linear-to-CIE1931 transformation using the matrix
    *X = norm_r * RGB_to_XYZ[0][0] + norm_g * RGB_to_XYZ[0][1] + norm_b * RGB_to_XYZ[0][2];
    *Y = norm_r * RGB_to_XYZ[1][0] + norm_g * RGB_to_XYZ[1][1] + norm_b * RGB_to_XYZ[1][2];
    *Z = norm_r * RGB_to_XYZ[2][0] + norm_g * RGB_to_XYZ[2][1] + norm_b * RGB_to_XYZ[2][2];
}



/**
 * @brief linear interpolate between two floats
 * 
 * @param x value A
 * @param y value B
 * @param a Normal 0-1 interpolation amount
 * @return float 
 */
__attribute__((pure))
float mixf(const float x, const float y, const Normal a) {
    return x * (1.0f - a) + y * a;
}

/**
 * @brief  clamp a value between >= lower and <= upper
 * 
 * @param x value to clamp
 * @param lower  lower bound inclusive
 * @param upper  upper bound inclusive
 * @return float 
 */
__attribute__((pure))
float clampf(const float x, const float lower, const float upper) {
	return fmaxf(lower, fminf(x, upper));
}



__attribute__((pure, hot))
inline uint8_t saturating_add(uint8_t a, int8_t b) {
    int16_t result = (int16_t)a + (int16_t)b;  // Cast to a larger type to avoid overflow
    if (result > 255) {
        return 255;  // Clamp to max value of uint8_t
    } else if (result < 0) {
        return 0;    // Clamp to min value of uint8_t
    } else {
        return (uint8_t)result;
    }
}

// calculate the number of bits required to store a number of size max_value
__attribute__((pure, hot))
inline uint8_t quant_bits(const uint8_t max_value) {
    int bits = 0;
    uint8_t bits_remaining = max_value;
    // calculate number of bits required to store a number of size num_bits
    while (bits_remaining > 0) {
        bits++;
        bits_remaining >>= 1;
    }
    return bits;
}


// return a mask of just the upper bits number of bits (ie if bits is 5 return 0xF8)
__attribute__((pure, hot))
inline uint8_t quant_mask(const uint8_t max_value) {
    int bits = quant_bits(max_value);

    return (1 << bits) - 1;
}

// normalize a uint8_t to a Normalized float
__attribute__((pure))
Normal normalize8(const uint8_t in) {
	return (Normal)(float)in / 255.0f;
}

// calculate the luminance of a color, return as a normal 0-1
//Normal luminance(const Normal red, const Normal green, const Normal blue) {
__attribute__((pure))
Normal luminance(const RGBF *__restrict__ in) {
    Normal result = 0.299f * in->r + 0.587f * in->g + 0.114f * in->b;
    ASSERT(result >= 0.0f && result <= 1.0f);
    return result;
}



/**
 * @brief adjust the contrast and saturation of an RGBF pixel
 * 
 * @param in this RGBF value will be adjusted in place. no new RGBF value is returned
 * @param contrast - contrast value 0-1
 * @param saturation - saturation value 0-1
 */
void adjust_contrast_saturation_inplace(RGBF *__restrict__ in, const float contrast, const float saturation) {
	Normal lum  = luminance(in);

    // Adjust saturation: move the color towards or away from the grayscale value
    Normal red   = mixf(lum, in->r, saturation);
    Normal green = mixf(lum, in->g, saturation);
    Normal blue  = mixf(lum, in->b, saturation);

    // Adjust contrast: scale values around 0.5
    red   = (red   - 0.5f) * contrast + 0.5f;
    green = (green - 0.5f) * contrast + 0.5f;
    blue  = (blue  - 0.5f) * contrast + 0.5f;

    // Clamp values between 0 and 1 (maybe optimize with simple maxf?)
    in->r = clampf(red, 0.0f, 1.0f);
    in->g = clampf(green, 0.0f, 1.0f);
    in->b = clampf(blue, 0.0f, 1.0f);
}

/**
 * @brief  perform gamma correction on a single byte value (0-255)
 * 
 * @param x - value to gamma correct
 * @param gamma  - gamma correction factor, 1.0 - 2.4 is typical
 * @return uint8_t  - the gamma corrected value
 */
__attribute__((pure))
inline uint8_t byte_gamma_correct(const uint8_t x, const float gamma) {
    Normal normal = normalize8(x);
    Normal correct = normal_gamma_correct(normal, gamma);
    return (uint8_t)MAX(0, MIN((correct * 254.0f), 254));
}

__attribute__((pure))
inline Normal normal_gamma_correct(const Normal x, const float gamma) {
    //return powf(x, 1.0f / gamma);
    ASSERT(x >= 0.0f && x <= 1.0f);
    return powf(x, gamma);
}

// Tone map function using ACES
__attribute__((pure))
inline Normal aces_tone_map(const Normal color) {
    return (color * (ACES_A * color + ACES_B)) / (color * (ACES_C * color + ACES_D) + ACES_E);
}

// Tone map function using ACES
__attribute__((pure))
inline Normal reinhard_tone_map(const Normal color) {
    return color / (1.0f + color);
}

// Hable's Uncharted 2 Tone Mapping function
__attribute__((pure))
inline Normal hable_tone_map(const Normal color) {
    float mapped_color = ((color * (UNCHART_A * color + UNCHART_C * UNCHART_B) + UNCHART_D * UNCHART_E) / (color * (UNCHART_A * color + UNCHART_B) + UNCHART_D * UNCHART_F)) - UNCHART_E / UNCHART_F;
    return mapped_color;
}

/**
 * @brief perform ACES tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
inline void aces_tone_mapper8(const RGB *__restrict__ in, RGB *__restrict__ out) {
    out->r = (uint8_t)(aces_tone_map(normalize8(in->r)) * 255);
    out->g = (uint8_t)(aces_tone_map(normalize8(in->g)) * 255);
    out->b = (uint8_t)(aces_tone_map(normalize8(in->b)) * 255);
}

/**
 * @brief perform ACES tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
inline void aces_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out) {
    out->r = aces_tone_map(in->r);
    out->g = aces_tone_map(in->g);
    out->b = aces_tone_map(in->b);
}



/**
 * @brief perform HABLE Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
inline void hable_tone_mapper(const RGB *__restrict__ in, RGB *__restrict__ out) {
    out->r = (uint8_t)(hable_tone_map(normalize8(in->r)) * 255);
    out->g = (uint8_t)(hable_tone_map(normalize8(in->g)) * 255);
    out->b = (uint8_t)(hable_tone_map(normalize8(in->b)) * 255);
}

/**
 * @brief perform HABLE Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
inline void hable_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out) {
    out->r = hable_tone_map((in->r));
    out->g = hable_tone_map((in->g));
    out->b = hable_tone_map((in->b));
}


/**
 * @brief perform HABLE Uncharted 2 tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
inline void reinhard_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out) {
    out->r = reinhard_tone_map((in->r));
    out->g = reinhard_tone_map((in->g));
    out->b = reinhard_tone_map((in->b));
}



/**
 * @brief perform ACES tone mapping for a single pixel
 * 
 * @param in pointer to the input RGB 
 * @param out pointer to the output RGB 
 */
void reinhard_tone_mapper(const RGB *__restrict__ in, RGB *__restrict__ out) {
    out->r = (uint8_t)(reinhard_tone_map(normalize8(in->r)) * 255);
    out->g = (uint8_t)(reinhard_tone_map(normalize8(in->g)) * 255);
    out->b = (uint8_t)(reinhard_tone_map(normalize8(in->b)) * 255);
}

/**
 * @brief an empty tone mapper that does nothing
 * 
 * @param in 
 * @param out 
 */
void copy_tone_mapperF(const RGBF *__restrict__ in, RGBF *__restrict__ out) {
    out->r = in->r;
    out->g = in->g;
    out->b = in->b;
}



/**
 * @brief map an input byte to a 32 bit pwm signal
 * 
 */
__attribute__((cold, pure))
uint32_t byte_to_pwm32(const uint8_t input, const uint8_t num_bits) {
    ASSERT((num_bits <= 32));

    // Calculate the number of '1's in the 11-bit result based on the 8-bit input
    uint32_t num_ones = (input * num_bits) / 255;  // Map 0-255 input to 0-num_bits ones
    uint32_t pwm_signal = 0;

    // dont divide by 0!
    if (num_ones == 0) {
        return pwm_signal;
    }

    float step = (float)num_bits / (float)num_ones;  // Step for evenly distributing 1's
    for (uint16_t i = 0; i < num_ones; i++) {
        int shift = (int)(i * step);
        pwm_signal |= (1 << (shift));
    }

    return pwm_signal;
}


/**
 * @brief map an input byte to a 64 bit pwm signal
 * 
 */
__attribute__((cold, pure))
uint64_t byte_to_pwm64(const uint8_t input, const uint8_t num_bits) {
    ASSERT(num_bits <= 64);

    // Calculate the number of '1's in the 11-bit result based on the 8-bit input
    uint16_t i1 = input;  // make sure we use at least 16 bits for the multiplication
    uint8_t num_ones = (i1 * num_bits) / 255;  // Map 0-255 input to 0-num_bits ones
    uint64_t pwm_signal = 0;

    // dont divide by 0!
    // lets make sure we get some 1s in there!
    if (num_ones == 0) {
        num_ones++;
        if (input < 1) {
            return pwm_signal;
        }
    }

    float step = (float)num_bits / (float)num_ones;  // Step for evenly distributing 1's
    // printf("[%d] num 1: [%d] step: %f\n", i1, num_ones, step);
    for (uint16_t i = 0; i < num_ones; i++) {
        int shift = (int)(i * step);
        pwm_signal |= (1ULL << (shift));
    }

    return pwm_signal;
}


/**
 * @brief  map 2 pixels to pwm signal using classic bit plane method
 * 
 * 
 * I have experimented with intrinsics and NEON, but the code was only a bit faster
 * NEON was about .45ms faster per frame, or about 33-40ms savings per second.  
 * since we have a full 10ms to spare at 75fps, i don't think it's worth the hassle to maintain.
 * 
 * @param pwm_signal 
 * @param scene 
 * @param pixel_upper 
 * @param pixel_lower 
 */
__attribute__((hot))
inline void update_pwm_signal_classic(uint32_t *__restrict__ pwm_signal, const uint8_t bit_depth, const RGB *__restrict__ pixel_upper, const RGB *__restrict__ pixel_lower, const uint8_t port_off, const uint64_t *__restrict__ bits, const uint32_t pwm_clear_mask) {

    const static uint32_t PORT[24] = {
        (1 << ADDRESS_P0_R1), (1 << ADDRESS_P0_R2), (1 << ADDRESS_P0_G1), (1 << ADDRESS_P0_G2), (1 << ADDRESS_P0_B1), (1 << ADDRESS_P0_B2),
        (1 << ADDRESS_P0_R1), (1 << ADDRESS_P0_R2), (1 << ADDRESS_P0_G1), (1 << ADDRESS_P0_G2), (1 << ADDRESS_P0_B1), (1 << ADDRESS_P0_B2),
        (1 << ADDRESS_P1_R1), (1 << ADDRESS_P1_R2), (1 << ADDRESS_P1_G1), (1 << ADDRESS_P1_G2), (1 << ADDRESS_P1_B1), (1 << ADDRESS_P1_B2),
        (1 << ADDRESS_P2_R1), (1 << ADDRESS_P2_R2), (1 << ADDRESS_P2_G1), (1 << ADDRESS_P2_G2), (1 << ADDRESS_P2_B1), (1 << ADDRESS_P2_B2)
    };

    // cant compute this in the loop or it confuses the vectorizer....
    const static uint64_t mask_lookup[80] = {
        1ULL << 0, 1ULL << 1, 1ULL << 2, 1ULL << 3, 1ULL << 4, 1ULL << 5, 1ULL << 6, 1ULL << 7,
        1ULL << 8, 1ULL << 9, 1ULL << 10, 1ULL << 11, 1ULL << 12, 1ULL << 13, 1ULL << 14, 1ULL << 15,
        1ULL << 16, 1ULL << 17, 1ULL << 18, 1ULL << 19, 1ULL << 20, 1ULL << 21, 1ULL << 22, 1ULL << 23,
        1ULL << 24, 1ULL << 25, 1ULL << 26, 1ULL << 27, 1ULL << 28, 1ULL << 29, 1ULL << 30, 1ULL << 31,
        1ULL << 32, 1ULL << 33, 1ULL << 34, 1ULL << 35, 1ULL << 36, 1ULL << 37, 1ULL << 38, 1ULL << 39,
        1ULL << 40, 1ULL << 41, 1ULL << 42, 1ULL << 43, 1ULL << 44, 1ULL << 45, 1ULL << 46, 1ULL << 47,
        1ULL << 48, 1ULL << 49, 1ULL << 50, 1ULL << 51, 1ULL << 52, 1ULL << 53, 1ULL << 54, 1ULL << 55,
        1ULL << 56, 1ULL << 57, 1ULL << 58, 1ULL << 59, 1ULL << 60, 1ULL << 61, 1ULL << 62, 1ULL << 63
    };



    // bits are pre-computed in a lookup table for the pwm values for each pixel value
    // each bit plane is 32 bits long and computed separately for each color channel
    uint64_t r1_pwm = bits[pixel_upper->r];
    uint64_t r2_pwm = bits[pixel_lower->r];
    uint64_t g1_pwm = bits[pixel_upper->g+256];
    uint64_t g2_pwm = bits[pixel_lower->g+256];
    uint64_t b1_pwm = bits[pixel_upper->b+512];
    uint64_t b2_pwm = bits[pixel_lower->b+512];

    const uint32_t *port = PORT+ port_off;

    uint8_t offset = 0;
    for (int j=0; j<bit_depth; j++) {
        // mask off just this bit plane's data
        uint64_t mask = mask_lookup[j];

        // clear just the bits on this port for this bit plane so we don't accidentally clear 
        // bits for the other rgb ports
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
}





/**
 * @brief create a lookup table for the pwm values for each pixel value
 * applies gamma correction and tone mapping based on the settings passed in scene
 * scene->brightness, scene->gamma, scene->red_linear, scene->green_linear, scene->blue_linear
 * scene->tone_mapper
 * 
 */
__attribute__((cold))
uint64_t *tone_map_rgb_bits(const scene_info *scene) {

    size_t bytes = 3 * 264 * sizeof(uint64_t);
    _Alignas(64) uint64_t *bits = (uint64_t*)aligned_alloc(64, bytes);
    memset(bits, 0, bytes);
    //printf("green linear: %f, blue linear: %f", (double)scene->green_linear, (double)scene->blue_linear);
    uint8_t brightness = (scene->jitter_brightness) ? 255 : scene->brightness;
    for (uint16_t i=0; i<=255; i++) {
        RGBF tone_pixel = {0, 0 , 0};
        RGBF gamma_pixel = {
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->red_linear, scene->gamma*scene->red_gamma),
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->green_linear, scene->gamma*scene->green_gamma),
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->blue_linear, scene->gamma*scene->blue_gamma)
        };
        // printf("i: %d: g: %f = O: %f\n", i, (double)g, (double)gamma_pixel.g);

        // tone map...
        if (scene->tone_mapper != NULL) {
            scene->tone_mapper(&gamma_pixel, &tone_pixel);
        } else {
            tone_pixel.r = gamma_pixel.r;
            tone_pixel.g = gamma_pixel.g;
            tone_pixel.b = gamma_pixel.b;
        }

        bits[i]     = byte_to_pwm64(MIN(tone_pixel.r * brightness, 255), scene->bit_depth);
        bits[i+256] = byte_to_pwm64(MIN(tone_pixel.g * brightness, 255), scene->bit_depth);
        bits[i+512] = byte_to_pwm64(MIN(tone_pixel.b * brightness, 255), scene->bit_depth);

        if (CONSOLE_DEBUG) {
            debug("i: %d, Scale: %f,  Gamma Corrected Scaled: %f, Tone Mapped pwm: ", i, (double)RED_SCALE, (double)gamma_pixel.r, (double)tone_pixel.r);
            binary64(stderr, bits[i]);
            debug("\n");
        }
    }

    return bits;
} 


/**
 * @brief this function takes the image data and maps it to the pwm signal. scene->stride is the number of bytes per pixel.
 * for 24bpp RGB images, this should be set to 3, for 32bpp RGBA images this should be set to 4.
 * 
 */
__attribute__((hot))
void map_byte_image_to_pwm(uint8_t *restrict image, const scene_info *scene, const uint8_t do_fps_sync) {

    const static uint32_t PORT[24] = {
        (1 << ADDRESS_P0_R1), (1 << ADDRESS_P0_R2), (1 << ADDRESS_P0_G1), (1 << ADDRESS_P0_G2), (1 << ADDRESS_P0_B1), (1 << ADDRESS_P0_B2),
        (1 << ADDRESS_P0_R1), (1 << ADDRESS_P0_R2), (1 << ADDRESS_P0_G1), (1 << ADDRESS_P0_G2), (1 << ADDRESS_P0_B1), (1 << ADDRESS_P0_B2),
        (1 << ADDRESS_P1_R1), (1 << ADDRESS_P1_R2), (1 << ADDRESS_P1_G1), (1 << ADDRESS_P1_G2), (1 << ADDRESS_P1_B1), (1 << ADDRESS_P1_B2),
        (1 << ADDRESS_P2_R1), (1 << ADDRESS_P2_R2), (1 << ADDRESS_P2_G1), (1 << ADDRESS_P2_G2), (1 << ADDRESS_P2_B1), (1 << ADDRESS_P2_B2)
    };


	// get the current time and estimate fps delay
    uint32_t frame_time_us = 1000000 / scene->fps;
    static struct timespec prev_time, cur_time;

    // lets tone map the bits for the current scene, update if the lookup table if scene tone mapping changes....
    static uint64_t *bits = NULL;
    static func_tone_mapper_t tone_map = NULL;
    if (bits == NULL || tone_map != scene->tone_mapper) {
        if (bits != NULL) { // don't leak memory!
            free(bits);
        }   
        bits = tone_map_rgb_bits(scene);
        tone_map = scene->tone_mapper;
    }

    // map the image to handle weird panel chain configurations
    if (scene->image_mapper != NULL) {
        debug("calling image mapper %p\n", scene->image_mapper);
        scene->image_mapper(image, NULL, scene);
    }


    // we need some variables to allow us to iterate over the image in various ways
    // mirroring is just iterating over the image in reverse order
    // port mirroring can be done by switching the port order on the board
    int16_t bitmask_start = 0, img_y = 0;
    int32_t bitmask_stride = scene->bit_depth;
    if (scene->image_mirror)  {
        bitmask_stride = -scene->bit_depth;
        bitmask_start = scene->width - 1;
    }
    for (uint8_t port_num = 0; port_num < scene->num_ports;  port_num++) {
        const uint32_t port_off = (port_num * 6);
        uint32_t pwm_clear_mask = ~(PORT[port_off+0] | PORT[port_off+1] | PORT[port_off+2] | PORT[port_off+3] | PORT[port_off+4] | PORT[port_off+5]);

        for (int panel_y=0; panel_y < (scene->panel_height/2); panel_y++) {

            int pwm_offset = (panel_y * scene->width + bitmask_start) * scene->bit_depth;
            int offset1 = (img_y * scene->width) * scene->stride;
            int offset2 = ((img_y + 32) * scene->width) * scene->stride;
            for (int x=0; x < scene->width; x++) {

                const RGB *__restrict__ pixel_upper = (RGB *)(image + offset1);
                const RGB *__restrict__ pixel_lower = (RGB *)(image + offset2);

                update_pwm_signal_classic(scene->pwm_signalA + pwm_offset, scene->bit_depth, pixel_upper, pixel_lower, port_off, bits, pwm_clear_mask);
                pwm_offset += bitmask_stride;
                offset1 += scene->stride;
                offset2 += scene->stride;
            }
            img_y++;
        }
        // need to skip over the lower half of the panel we already mapped in...
        img_y+=scene->panel_height/2;
    }
    
	// Calculate the sleep delay
	if (do_fps_sync && prev_time.tv_sec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &cur_time);
		long frame_time = (cur_time.tv_sec - prev_time.tv_sec) * 1000000L + (cur_time.tv_nsec - prev_time.tv_nsec) / 1000L;

		// Sleep for the remainder of the frame time to achieve 120 FPS
		if (frame_time < frame_time_us) {
			usleep(frame_time_us - frame_time);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &prev_time);
}




// map (image_width x panel height) blocks to pwm signal
//void map_byte_image_to_pwm(uint8_t *image, uint32_t *pwm_signal, const uint8_t port_num, const scene_info *scene) {
__attribute__((hot))
void map_byte_image_to_pwm_dither(uint8_t *restrict image, const scene_info *scene, const uint8_t do_fps_sync) {


    const static uint32_t PORT[18] = {
        (1 << ADDRESS_P0_R1), (1 << ADDRESS_P0_R2), (1 << ADDRESS_P0_G1), (1 << ADDRESS_P0_G2), (1 << ADDRESS_P0_B1), (1 << ADDRESS_P0_B2),
        (1 << ADDRESS_P1_R1), (1 << ADDRESS_P1_R2), (1 << ADDRESS_P1_G1), (1 << ADDRESS_P1_G2), (1 << ADDRESS_P1_B1), (1 << ADDRESS_P1_B2),
        (1 << ADDRESS_P2_R1), (1 << ADDRESS_P2_R2), (1 << ADDRESS_P2_G1), (1 << ADDRESS_P2_G2), (1 << ADDRESS_P2_B1), (1 << ADDRESS_P2_B2)
    };


	// get the current time and estimate fps delay
    int16_t inc_y = 1, img_y = 0;
    uint32_t frame_time_us = 1000000 / scene->fps;
    static struct timespec prev_time, cur_time;

    // lets tone map the bits for the current scene, update if the tone mapping changes....
    static uint64_t *bits = NULL;
    static func_tone_mapper_t tone_map = NULL;
    if (bits == NULL || tone_map != scene->tone_mapper) {
        if (bits != NULL) { // don't leak memory!
            free(bits);
        }   
        printf("map pwm bits\n");
        bits = tone_map_rgb_bits(scene);
        tone_map = scene->tone_mapper;
    }

    if (scene->invert) {
        img_y = scene->height - 1;
        inc_y = -1;
    }



    for (uint8_t port_num = 0; port_num < scene->num_ports;  port_num++) {
        const uint32_t port_off = (port_num * 6);
        const uint32_t pwm_clear_mask = ~(PORT[port_off+0] | PORT[port_off+1] | PORT[port_off+2] | PORT[port_off+3] | PORT[port_off+4] | PORT[port_off+5]);

        for (int panel_y=0; panel_y < (scene->panel_height/2); panel_y++) {

            int pwm_offset = (panel_y * scene->width) * scene->bit_depth;
            int offset1 = (img_y * scene->width) * scene->stride;
            int offset2 = ((img_y + 32) * scene->width) * scene->stride;
            for (int x=0; x < scene->width; x++) {

                const RGB *__restrict__ pixel_upper = (RGB *)(image + offset1);
                const RGB *__restrict__ pixel_lower = (RGB *)(image + offset2);

                update_pwm_signal_classic(scene->pwm_signalA + pwm_offset, scene->bit_depth, pixel_upper, pixel_lower, port_off, bits, pwm_clear_mask);

                pwm_offset += scene->bit_depth;
                offset1 += scene->stride;
                offset2 += scene->stride;
            }
            img_y += inc_y;
        }
        // need to skip over the lower half of the panel we already mapped in...
        img_y+=inc_y * (scene->panel_height/2);
    }
    
	// Calculate the sleep delay
	if (__builtin_expect((do_fps_sync && prev_time.tv_sec == 0), 1)) {
		clock_gettime(CLOCK_MONOTONIC, &cur_time);
		long frame_time = (cur_time.tv_sec - prev_time.tv_sec) * 1000000L + (cur_time.tv_nsec - prev_time.tv_nsec) / 1000L;

		// Sleep for the remainder of the frame time to achieve 120 FPS
		if (frame_time < frame_time_us) {
			usleep(frame_time_us - frame_time);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &prev_time);
}



/**
 * @brief map the lower half of the image to the front of the image. this allows connecting
 * panels in a left, left, down, right pattern (or right, right, down, left) if the image is
 * mirrored
 * 
 * 
 * @param image - input buffer to map
 * @param output_image - if NULL, the output buffer will be allocated for you
 * @param scene - the scene information
 * @return uint8_t* - pointer to the output buffer
 */
image_mapper_t u_mapper_impl;
uint8_t *u_mapper_impl(const uint8_t *image_in, uint8_t *image_out, const struct scene_info *scene) {
    static uint8_t *output_image = NULL;
    if (output_image == NULL) {
        debug("Allocating memory for u_mapper\n"); 
        output_image = (uint8_t*)aligned_alloc(64, scene->width * scene->height * scene->stride);
        if (output_image == NULL) {
            die("Failed to allocate memory for u_mapper image\n");
        }
    }
    if (image_out == NULL) {
        debug("output image is NULL, using allocated memory\n");
        output_image = output_image;
    }


    // Split image into top and bottom halves
    const uint8_t *bottom_half = image_in + (scene->width * (scene->height / 2) * scene->stride);  // Last 64 rows
    const uint32_t row_length = scene->width * scene->stride;

    debug("width: %d, stride: %d, row_length: %d", scene->width, scene->stride, row_length);
    // Remap bottom half to the first part of the output
    for (int y = 0; y < (scene->height / 2); y++) {
        // Copy each row from bottom half
        debug ("  Y: %d, offset: %d", y, y * scene->width * scene->stride);
        memcpy(output_image + (y * scene->width * scene->stride), bottom_half + (y * scene->width * scene->stride), row_length);
    }

    // Remap top half to the second part of the output
    for (int y = 0; y < (scene->height / 2); y++) {
        // Copy each row from top half
        memcpy(output_image + ((y + (scene->width / 2)) * scene->width * scene->stride), image_in + (y * scene->width * scene->stride), row_length);
    }

    return output_image;
}
//func_image_mapper_t u_mapper = u_mapper_impl;



/**
 * @brief draw a random square on image
 * 
 * @param image buffer to draw to
 * @param img_width buffer width in pixels
 * @param img_height buffer height in pixels
 * @param stride number of BYTES per pixel (usually 3 or 4)
 */
void draw_square(uint8_t *image, const uint16_t img_width, const uint16_t img_height, const uint8_t stride) {

    int width  = rand() % 16 + 4;;
    int height = rand() % 16 + 4;;

    int x_start = rand() % (img_width - width);
    int y_start = rand() % (img_height - height);


    uint8_t red = rand() % 256;
    uint8_t green = rand() % 256;
    uint8_t blue = rand() % 256;

    //RGB color = {red, green, blue};
    //hable_inplace(&color);

    // Draw the rectangle
    for (int y = y_start; y < y_start + height; y++) {
        for (int x = x_start; x < x_start + width; x++) {
            int o = (y * img_width + x) * stride;
            image[o+0] = red;//color.r;  // Set red channel
            image[o+1] = green;//color.g;  // Set green channel
            image[o+2] = blue;//color.b;  // Set blue channel
        }
    }
}


void dither_image(uint8_t *image, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Original color (extract 5-bit per channel components)
            //uint16_t original = image[y * width + x];
	    uint16_t offset = y * width + x;
            uint8_t r = (image[offset]) & 0xF8; // 5-bit red
            uint8_t g = (image[offset+1]) & 0xF8;  // 5-bit green
            uint8_t b = image[offset+2] & 0xF8;        // 5-bit blue

            // Compute quantized color
            uint8_t quant_r = (r >> 1) << 1; // Quantized red
            uint8_t quant_g = (g >> 1) << 1; // Quantized green
            uint8_t quant_b = (b >> 1) << 1; // Quantized blue

            // Compute error
            int16_t err_r = image[offset] - quant_r;
            int16_t err_g = image[offset+1] - quant_g;
            int16_t err_b = image[offset+2] - quant_b;

            // Assign quantized color back to the image
            image[y * width + x] = (quant_r << 10) | (quant_g << 5) | quant_b;

            // Distribute the error to neighboring pixels
            if (x + 1 < width) {

                // image[y * width + (x + 1)] = __qadd(image[y*width + (x+1)], err_r * 7 / 16);
                //image[y * width + (x + 2)] = __ssat(image[y*width + (x+2)], err_g * 7 / 16);
                //image[y * width + (x + 3)] = __ssat(image[y*width + (x+3)], err_b * 7 / 16);
                //image[y * width + (x + 1)] += err_g * 7 / 16;
                //image[y * width + (x + 1)] += err_b * 7 / 16;
            }
            if (y + 1 < height) {
                if (x - 1 >= 0) {
                    image[(y + 1) * width + (x - 1)] += err_r * 3 / 16;
                    image[(y + 1) * width + (x - 1)] += err_g * 3 / 16;
                    image[(y + 1) * width + (x - 1)] += err_b * 3 / 16;
                }
                image[(y + 1) * width + x] += err_r * 5 / 16;
                image[(y + 1) * width + x] += err_g * 5 / 16;
                image[(y + 1) * width + x] += err_b * 5 / 16;
                if (x + 1 < width) {
                    image[(y + 1) * width + (x + 1)] += err_r * 1 / 16;
                    image[(y + 1) * width + (x + 1)] += err_g * 1 / 16;
                    image[(y + 1) * width + (x + 1)] += err_b * 1 / 16;
                }
            }
        }
    }
}


// Function to apply noise and quantize to 5 bits per channel
uint8_t quantize_with_noise(uint8_t color) {
    // Add random noise (scaled between -0.5 and 0.5)
    float noise = (rand() % 1000 / 1000.0f) - 0.5f;

    // Normalize color and apply noise
    float normalized = clampf(normalize8(color) + noise, 0.0f, 1.0f);

    // Scale to 5-bit and return
    return (uint8_t)(normalized * 31.0f);
}

// Apply dithering to an image
void apply_noise_dithering(uint8_t *image, int width, int height) {
    for (int i = 0; i < width * height * 3; i += 3) {
        // Original RGB values (24bpp)
        uint8_t r = image[i];
        uint8_t g = image[i + 1];
        uint8_t b = image[i + 2];

        // Apply noise dithering and quantize to 5 bits
        uint8_t r5 = quantize_with_noise(r);
        uint8_t g5 = quantize_with_noise(g);
        uint8_t b5 = quantize_with_noise(b);

        // Combine into a 15-bit RGB value (5 bits per channel)
        image[i] = r5;
        image[i + 1] = g5;
        image[i + 2] = b5;
        //output_image[i / 3] = (r5 << 10) | (g5 << 5) | b5;
    }
}


/**
 * @brief helper method to set a pixel in a 24 bit image buffer
 * 
 * @param image pointer to the image to update
 * @param x horizontal position (starting at 0)
 * @param y vertical position (starting at 0)
 * @param r red value 0-255
 * @param g green value 0-255
 * @param b blue value 0-255
 */
inline void set_pixel24(uint8_t *image, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    assert(global_width > 0);

    int offset = (y * global_width + x) * 3;
    image[offset] = r;
    image[offset + 1] = g;
    image[offset + 2] = b;
}


/**
 * @brief helper method to set a pixel in a 24 bit image buffer
 * 
 * @param image pointer to the image to update
 * @param x horizontal position (starting at 0)
 * @param y vertical position (starting at 0)
 * @param r red value 0-255
 * @param g green value 0-255
 * @param b blue value 0-255
 */
inline void set_pixel32(uint8_t *image, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    assert(global_width > 0);

    int offset = (y * global_width + x) * 4;
    image[offset] = r;
    image[offset + 1] = g;
    image[offset + 2] = b;
}


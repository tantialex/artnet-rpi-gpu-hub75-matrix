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
#include <assert.h>


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
 * @brief interpolate between two colors
 * 
 */
__attribute__((hot))
void interpolate_rgb(RGB* result, const RGB start, const RGB end, const Normal ratio) {
    result->r = (uint8_t)(start.r + (end.r - start.r) * ratio);
    result->g = (uint8_t)(start.g + (end.g - start.g) * ratio);
    result->b = (uint8_t)(start.b + (end.b - start.b) * ratio);
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




///////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * @brief map an input byte to a 32 bit pwm signal
 * 
 */
__attribute__((cold, pure))
uint32_t byte_to_bcm32(const uint8_t input, const uint8_t num_bits) {
    ASSERT((num_bits <= 32));

    // Calculate the number of '1's in the 11-bit result based on the 8-bit input
    uint32_t num_ones = (input * num_bits) / 255;  // Map 0-255 input to 0-num_bits ones
    uint32_t bcm_signal = 0;

    // dont divide by 0!
    if (num_ones == 0) {
        if (input < 1) {
            return bcm_signal;
        }
    }
    num_ones++;

    float step = (float)num_bits / (float)num_ones;  // Step for evenly distributing 1's
    for (uint16_t i = 0; i < num_ones; i++) {
        int shift = (int)(i * step);
        bcm_signal |= (1 << (shift));
    }

    return bcm_signal;
}


/**
 * @brief map an input byte to a 64 bit pwm signal
 * 
 */
__attribute__((cold, pure))
uint64_t byte_to_bcm64(const uint8_t input, const uint8_t num_bits) {
    ASSERT(num_bits <= 64);

    // Calculate the number of '1's in the 11-bit result based on the 8-bit input
    uint16_t i1 = input;  // make sure we use at least 16 bits for the multiplication
    uint8_t num_ones = (i1 * num_bits) / 255;  // Map 0-255 input to 0-num_bits ones
    uint64_t bcm_signal = 0;

    // dont divide by 0!
    // make sure we get some 1s in there!
    if (num_ones == 0) {
        if (input < 1) {
            return bcm_signal;
        }
    }
    // don't output black unless the input is actually 0.
    // this ensures we always have at least 1 bit set
    num_ones++;

    // now we will calculate the step size and place a 1 every step bits
    float step = (float)num_bits / (float)num_ones;  // Step for evenly distributing 1's
    for (uint16_t i = 0; i < num_ones; i++) {
        int shift = (int)(i * step);
        bcm_signal |= (1ULL << (shift));
    }

    return bcm_signal;
}



/**
 * @brief map 6 pixels of to bcm data. supports 3 output ports with 2 pixels per port
 * 
 * @param scene the scene information
 * @param void_bits pointer to the gamma corrected tone mapped pwm data for each RGB value. (uint32_t !)
 * @param pwm_signal pointer to the bcm data for current X/Y. (y = 0 - panel_height/2), scene->bit_depth bytes will be updated here
 * @param image pointer to 24bpp RGB or 32bpp RGBA image data at the current pixel offset. IE: image[offset]
 */
__attribute__((hot))
void update_bcm_signal_32(
    const scene_info *scene,
    const void *__restrict__ void_bits,
    uint32_t *__restrict__ bcm_signal,
    const uint8_t *__restrict__ image) {

    const uint32_t *bits = (const uint32_t*)void_bits;
    // offset to the Port0 top pixel image
    //uint32_t img_idx = offset;
    // offset from top pixel to lower pixel in image data. 
    static uint32_t panel_stride = 0;
    // offsets for each pixel on each port
    static uint32_t p0t = 0, p0b = 0, p1t = 0, p1b = 0, p2t = 0, p2b =0;

    // calculate the image index to all 3 ports. we only need to do this once ever
    if (UNLIKELY(panel_stride == 0)) {
        panel_stride = scene->width * (scene->panel_height / 2) * scene->stride;
        p0b = p0t + panel_stride;
        p1t = p0b + panel_stride;
        p1b = p1t + panel_stride;
        p2t = p1b + panel_stride;
        p2b = p2t + panel_stride;
    }

    // inform compiler that bit depth is aligned, improves compiler optimization
    // BIT_DEPTH_ALIGNMENT should be multiple of 4, ideally 16. 
    uint8_t bit_depth __attribute__((aligned(BIT_DEPTH_ALIGNMENT))) = scene->bit_depth;

    ASSERT(bit_depth % BIT_DEPTH_ALIGNMENT == 0);
    ASSERT(bit_depth <= 32);

    uint8_t bcm_offset = 0;
    for (int j=0; j<bit_depth; j++) {
        // mask off just this bit plane's data
        const uint32_t mask = 1 << j;

        // this works by first finding the index into the red byte (+0) of the 24bpp source image
        // looking up the bcm bit mask value at that red color value (128 = 1010101010101..), logical AND with
        // the current bit position we are calculating (1<<j) and then shifting that bit (0 or 1) to the correct pin (ADDRESS_Px_CX)
        // and logical OR that value for the current bcm_signal offset.
        // repeat this for green (+1), blue (+2), and once for each pixel on each port

        // !! - first ! turns 00100000 into 0000000, second ! turns 000000 into 00000001
        // !! - first ! turns 00000000 into 0000001, second ! turns 000001 into 00000000
        // this way we get a 1 value for the mask of the (bcm_bits & mask) so we can << the correct number of bits

        bcm_signal[bcm_offset++] =
            // PORT 0, top pixel
            (!!(bits[image[0]] & mask)) << ADDRESS_P0_R1 |
            (!!(bits[image[1]] & mask)) << ADDRESS_P0_G1 |
            (!!(bits[image[2]] & mask)) << ADDRESS_P0_B1 |

            // PORT 0, bottom pixel
            (!!(bits[image[p0b+0]] & mask)) << ADDRESS_P0_R2 |
            (!!(bits[image[p0b+1]] & mask)) << ADDRESS_P0_G2 |
            (!!(bits[image[p0b+2]] & mask)) << ADDRESS_P0_B2 |

            // PORT 1, bottom pixel
            (!!(bits[image[p1t+0]] & mask)) << ADDRESS_P1_R1 |
            (!!(bits[image[p1t+1]] & mask)) << ADDRESS_P1_G1 |
            (!!(bits[image[p1t+2]] & mask)) << ADDRESS_P1_B1 |

            // PORT 1, bottom pixel
            (!!(bits[image[p1b+0]] & mask)) << ADDRESS_P1_R2 |
            (!!(bits[image[p1b+1]] & mask)) << ADDRESS_P1_G2 |
            (!!(bits[image[p1b+2]] & mask)) << ADDRESS_P1_B2 |

            // PORT 2, bottom pixel
            (!!(bits[image[p2t+0]] & mask)) << ADDRESS_P1_R1 |
            (!!(bits[image[p2t+1]] & mask)) << ADDRESS_P1_G1 |
            (!!(bits[image[p2t+2]] & mask)) << ADDRESS_P1_B1 |

            // PORT 2, bottom pixel
            (!!(bits[image[p2b+0]] & mask)) << ADDRESS_P2_R2 |
            (!!(bits[image[p2b+1]] & mask)) << ADDRESS_P2_G2 |
            (!!(bits[image[p2b+2]] & mask)) << ADDRESS_P2_B2;

    }
    // bcm_signal is now bit mask of length bit_depth for these 6 pixels that can be iterated through to light
    // the LEDS to the correct brightness levels
}


__attribute__((hot))
void update_bcm_signal_64(
    const scene_info *scene,
    const void *__restrict__ void_bits,
    uint32_t *__restrict__ bcm_signal,
    const uint8_t *__restrict__ image) {

    const uint64_t *bits = (const uint64_t*)void_bits;
    // offset from top pixel to lower pixel in image data. 
    static uint32_t panel_stride = 0;
    // offsets for each pixel on each port
    static uint32_t p0t = 0, p0b = 0, p1t = 0, p1b = 0, p2t = 0, p2b =0;

    // calculate the image index to all 3 ports. we only need to do this once ever
    if (UNLIKELY(panel_stride == 0)) {
        panel_stride = scene->width * (scene->panel_height / 2) * scene->stride;
        p0b = p0t + panel_stride;
        p1t = p0b + panel_stride;
        p1b = p1t + panel_stride;
        p2t = p1b + panel_stride;
        p2b = p2t + panel_stride;
    }

    // inform compiler that bit depth is aligned, improves compiler optimization
    // BIT_DEPTH_ALIGNMENT should be multiple of 4, ideally 16. 
    uint8_t bit_depth __attribute__((aligned(BIT_DEPTH_ALIGNMENT))) = scene->bit_depth;

    ASSERT(bit_depth % BIT_DEPTH_ALIGNMENT == 0);
    ASSERT(bit_depth <= 32);

    uint8_t bcm_offset = 0;
    for (int j=0; j<bit_depth; j++) {
        // mask off just this bit plane's data
        const uint64_t mask = 1ULL << j;

        // this works by first finding the index into the red byte (+0) of the 24bpp source image
        // looking up the bcm bit mask value at that red color value (128 = 1010101010101..), logical AND with
        // the current bit position we are calculating (1<<j) and then shifting that bit (0 or 1) to the correct pin (ADDRESS_Px_CX)
        // and logical OR that value for the current bcm_signal offset.
        // repeat this for green (+1), blue (+2), and once for each pixel on each port

        // !! - first ! turns 00100000 into 0000000, second ! turns 000000 into 00000001
        // !! - first ! turns 00000000 into 0000001, second ! turns 000001 into 00000000
        // this way we get a 1 value for the mask of the (bcm_bits & mask) so we can << the correct number of bits

        bcm_signal[bcm_offset++] =
            // PORT 0, top pixel
            (!!(bits[image[0]] & mask)) << ADDRESS_P0_R1 |
            (!!(bits[image[1]] & mask)) << ADDRESS_P0_G1 |
            (!!(bits[image[2]] & mask)) << ADDRESS_P0_B1 |

            // PORT 0, bottom pixel
            (!!(bits[image[p0b+0]] & mask)) << ADDRESS_P0_R2 |
            (!!(bits[image[p0b+1]] & mask)) << ADDRESS_P0_G2 |
            (!!(bits[image[p0b+2]] & mask)) << ADDRESS_P0_B2 |

            // PORT 1, bottom pixel
            (!!(bits[image[p1t+0]] & mask)) << ADDRESS_P1_R1 |
            (!!(bits[image[p1t+1]] & mask)) << ADDRESS_P1_G1 |
            (!!(bits[image[p1t+2]] & mask)) << ADDRESS_P1_B1 |

            // PORT 1, bottom pixel
            (!!(bits[image[p1b+0]] & mask)) << ADDRESS_P1_R2 |
            (!!(bits[image[p1b+1]] & mask)) << ADDRESS_P1_G2 |
            (!!(bits[image[p1b+2]] & mask)) << ADDRESS_P1_B2 |

            // PORT 2, bottom pixel
            (!!(bits[image[p2t+0]] & mask)) << ADDRESS_P1_R1 |
            (!!(bits[image[p2t+1]] & mask)) << ADDRESS_P1_G1 |
            (!!(bits[image[p2t+2]] & mask)) << ADDRESS_P1_B1 |

            // PORT 2, bottom pixel
            (!!(bits[image[p2b+0]] & mask)) << ADDRESS_P2_R2 |
            (!!(bits[image[p2b+1]] & mask)) << ADDRESS_P2_G2 |
            (!!(bits[image[p2b+2]] & mask)) << ADDRESS_P2_B2;

    }
    // bcm_signal is now bit mask of length bit_depth for these 6 pixels that can be iterated through to light
    // the LEDS to the correct brightness levels
}




 
/**
 * @brief create a bcm signal map from linear sRGB space to the bcm(pwm) signal.
 * the returned pointer will be either uint32_t* or uint64_t* depending on the size
 * of num_bits.
 * 
 * each bcm entry will be a right aligned bit mask of length num_bits.
 * 
 * looking up any linear 8 bit value in the map will return a BCM bit mask of length num_bits
 * 
 * @param scene contains reference to jitter_brightness, gamma, 
 * brightness, red_linear, green_linear, blue_linear, red_gamma, green_gamma, blue_gamma, 
 * bit_depth, tone_mapper.
 * @param num_bits  number of bits of BCM data (good values from 8-64) try to make them multiples of 4 or 8
 * @return void* pointer to the bcm signal map. 0-255 red, 256-511 green, 512-767 blue
 */
void *tone_map_rgb_bits(const scene_info *scene, const int num_bits) {
    ASSERT(num_bits <= 64);

    //size_t bytes = 3 * 257 * sizeof(uint64_t);
    //_Alignas(64) uint64_t *bits = (uint64_t*)aligned_alloc(64, bytes);
    size_t bytes = 3 * 257 * sizeof(uint64_t);
    _Alignas(32) uint32_t *bits = (uint32_t*)aligned_alloc(32, bytes);
    memset(bits, 0, bytes);

    uint64_t *bits64 = (uint64_t *)bits;
    uint32_t *bits32 = (uint32_t *)bits;
    uint8_t brightness = (scene->jitter_brightness) ? 255 : scene->brightness;
    for (uint16_t i=0; i<=255; i++) {
        RGBF tone_pixel = {0, 0 , 0};
        RGBF gamma_pixel = {
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->red_linear, scene->gamma*scene->red_gamma),
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->green_linear, scene->gamma*scene->green_gamma),
            normal_gamma_correct(normalize8(MIN(i, 255)) * scene->blue_linear, scene->gamma*scene->blue_gamma)
        };
        // debug("i: %d: R:%f  G:%f B:%f\n", i, (double)gamma_pixel.r, (double)gamma_pixel.g, (double)gamma_pixel.b);

        // tone map the value ...
        if (scene->tone_mapper != NULL) {
            scene->tone_mapper(&gamma_pixel, &tone_pixel);
        } else {
            tone_pixel.r = gamma_pixel.r;
            tone_pixel.g = gamma_pixel.g;
            tone_pixel.b = gamma_pixel.b;
        }

        if (num_bits > 32 && num_bits <= 64) {
            bits64[i]     = byte_to_bcm64(MIN(tone_pixel.r * brightness, 255), scene->bit_depth);
            bits64[i+256] = byte_to_bcm64(MIN(tone_pixel.g * brightness, 255), scene->bit_depth);
            bits64[i+512] = byte_to_bcm64(MIN(tone_pixel.b * brightness, 255), scene->bit_depth);
        } else if (num_bits <= 32) {
            bits32[i]     = byte_to_bcm32(MIN(tone_pixel.r * brightness, 255), scene->bit_depth);
            bits32[i+256] = byte_to_bcm32(MIN(tone_pixel.g * brightness, 255), scene->bit_depth);
            bits32[i+512] = byte_to_bcm32(MIN(tone_pixel.b * brightness, 255), scene->bit_depth);

        }

        if (CONSOLE_DEBUG) {
            debug("i: %d, Scale: %f,  Gamma Corrected Scaled: %f, Tone Mapped pwm: ", i, (double)RED_SCALE, (double)gamma_pixel.r, (double)tone_pixel.r);
            if (num_bits <= 32) {
                uint32_t *num = (uint32_t *)bits;
                binary32(stderr, num[i]);
            } else {
                uint64_t *num = (uint64_t *)bits;
                binary64(stderr, num[i]);
            }
            debug("\n");
        }
    }

    if (num_bits <= 32) {
        printf("return bits32\n");
        return bits32;
    }
    return bits64;
}



/**
 * @brief this function takes the image data and maps it to the bcm signal.
 * 
 * if scene->tone_mapper is updated, new bcm bit masks will be created.
 * 
 * @param scene the scene information
 * @param image the image to map to the scene bcm data. if NULL scene->image will be used
 * @param do_fps_sync if true, the function will attempt to usleep delay sync to the scene->fps
 */
__attribute__((hot))
void map_byte_image_to_bcm(scene_info *scene, const uint8_t *image, const uint8_t do_fps_sync) {

	// get the current time and estimate fps delay
    const uint32_t frame_time_us = 1000000 / scene->fps;
    static struct timespec prev_time, cur_time;

    // tone map the bits for the current scene, update if the lookup table if scene tone mapping changes....
    static void *bits = NULL;
    static func_tone_mapper_t last_tone_map = NULL;

    if (UNLIKELY(bits == NULL || last_tone_map != scene->tone_mapper)) {
        if (bits != NULL) { // don't leak memory!
            free(bits);
        }   
        if (scene->bit_depth > 32) {
            bits = (uint64_t*)tone_map_rgb_bits(scene, scene->bit_depth);
        } else {
            bits = (uint32_t*)tone_map_rgb_bits(scene, scene->bit_depth);
        }
        last_tone_map = scene->tone_mapper;
        debug("new tone mapped bits created\n");
    }

    // map the image to handle weird panel chain configurations
    // the image mapper should take a normal image and map it to match the chain configuration
    // the image mapper should operate on the image in place
    if (scene->image_mapper != NULL) {
        debug("calling image mapper %p\n", scene->image_mapper);
        scene->image_mapper(image, NULL, scene);
    }

    const uint8_t *image_ptr = (image == NULL) ? scene->image : image;

    // use the correct bcm_signal mapper, 32 or 64 bit
    update_bcm_signal_fn update_bcm_signal = (scene->bit_depth > 32)
        ? (update_bcm_signal_fn)update_bcm_signal_64
        : (update_bcm_signal_fn)update_bcm_signal_32;

    ASSERT(scene->panel_height % 16 == 0);
    ASSERT(scene->panel_width % 16 == 0);
    // pwm_stride is the row length in bytes of the pwm output data
    //uint32_t pwm_stride __attribute__((aligned(BIT_DEPTH_ALIGNMENT))) = scene->width * scene->bit_depth;
    // const uint32_t pwm_stride = scene->width * scene->bit_depth;
    // half_height is 1/2 the panel height. since we clock in 2 pixels at a time, 
    // we only need to process half the rows
    const uint8_t  half_height __attribute__((aligned(16))) = scene->panel_height / 2;
    // ensure 16 bit alignment for width
    const uint16_t width __attribute__((aligned(32))) = scene->width;

    // ensure alignment for the compiler to optimize these loops
    ASSERT(scene->bit_depth % BIT_DEPTH_ALIGNMENT == 0);
    ASSERT(half_height % 16 == 0);
    ASSERT(pwm_stride % BIT_DEPTH_ALIGNMENT == 0);
    ASSERT(width % 32 == 0);                        // Ensure length is a multiple of 32
    // loop over 1/2 of the panel height
    uint32_t *bcm_signal = (scene->bcm_ptr)
        ? (scene->bcm_signalA)
        : (scene->bcm_signalB);

    for (uint16_t y=0; y < half_height; y++) {
        // for clarity: calculate the offset into the PWM buffer for the first pixel in this row
        //unsigned int pwm_offset = y * pwm_stride;

        for (uint16_t x=0; x < width; x++) {

            // this is just scene->bcm_signalA + bcm_offset, but we are telling the compiler it's aligned at BIT_DEPTH_ALIGNMENT
            update_bcm_signal(scene, bits, bcm_signal, image_ptr);

            bcm_signal += scene->bit_depth;
            image_ptr += scene->stride;
        }
    }

    // flip the double buffer. render_forever will detect this on next vsync and switch the buffers
    scene->bcm_ptr = !scene->bcm_ptr;
    
	// Calculate the sleep delay required to achieve the desired fps
	if (do_fps_sync && prev_time.tv_sec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &cur_time);
		long frame_time = (cur_time.tv_sec - prev_time.tv_sec) * 1000000L + (cur_time.tv_nsec - prev_time.tv_nsec) / 1000L;

		// Sleep for the remainder of the frame time to achieve scene->fps
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
 * This code is un-tested. If you have the time, please send me an implementation of U and V
 * mappers
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
        image_out = output_image;
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
// XXX readd to linux
//func_image_mapper_t u_mapper = u_mapper_impl;



/**
 * @brief this is a work in progress.  unfinished. do not use.  check back in the future.
 * 
 * @param image 
 * @param width 
 * @param height 
 */
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


float gradient_horiz(uint16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, float r0, float r1) {
    return r0;//(p1 - p3) / (p2 - p4);
}
float gradient_vert(uint16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, float r0, float r1) {
    return r1;//(p1 - p2) / (p3 - p4);
}
float gradient_max(uint16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, float r0, float r1) {
    return MAX(r0, r1);//(p1 - p2) / (p3 - p4);
}
float gradient_min(uint16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, float r0, float r1) {
    return MIN(r0, r1);//(p1 - p2) / (p3 - p4);
}
float gradient_quad(uint16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, float r0, float r1) {
    return (r0 < r1) ? r0 / r1 : r1 / r0;
}


/**
 * @brief helper method to set a pixel in a 24 bpp RGB image buffer
 * 
 * @param scene the scene to draw the pixel at
 * @param x horizontal position (starting at 0) clamped to scene->width
 * @param y vertical position (starting at 0) clamped to scene->height
 * @param pixel RGB value to set at pixel x,y
 */
inline void hub_pixel(scene_info *scene, const int x, const int y, const RGB pixel) {
    const uint16_t fx = MIN(x, scene->width-1);
    const uint16_t fy = MIN(y, scene->height-1);
    const int offset = (fy * scene->width + fx) * scene->stride;
    ASSERT(offset < scene->width * scene->height * scene->stride);

    scene->image[offset] = pixel.r;
    scene->image[offset + 1] = pixel.g;
    scene->image[offset + 2] = pixel.b;
}

/**
 * @brief helper method to set a pixel in a 24 bpp RGB image buffer, each
 * rgb channel is scaled by factor. if scaling exceeds byte storage (255)
 * the value will wrap. saturated artithmatic is still not portable....
 * 
 * @param scene the scene to draw the pixel at
 * @param x horizontal position (starting at 0)
 * @param y vertical position (starting at 0)
 * @param pixel RGB value to set at pixel x,y
 */
inline void hub_pixel_factor(scene_info *scene, const int x, const int y, const RGB pixel, const float factor) {
    const uint16_t fx = MIN(x, scene->width-1);
    const uint16_t fy = MIN(y, scene->height-1);
    const int offset = (fy * scene->width + fx) * scene->stride;
    ASSERT(offset < scene->width * scene->height * scene->stride);

    scene->image[offset] = pixel.r * factor;
    scene->image[offset + 1] = pixel.g * factor;
    scene->image[offset + 2] = pixel.b * factor;
}



/**
 * @brief helper method to set a pixel in a 32 bit RGBA image buffer
 * NOTE: You probably wand hub_pixel_factor for most cases
 * 
 * @param scene the scene to draw the pixel at
 * @param x horizontal position (starting at 0)
 * @param y vertical position (starting at 0)
 * @param pixel RGB value to set at pixel x,y
 */
inline void hub_pixel_alpha(scene_info *scene, const int x, const int y, const RGBA pixel) {
    const uint16_t fx = MIN(x, scene->width-1);
    const uint16_t fy = MIN(y, scene->height-1);
    const int offset = (fy * scene->width + fx) * scene->stride;
    ASSERT(scene->stride == 4);
    ASSERT(offset < scene->width * scene->height * scene->stride);

    Normal alpha = normalize8(pixel.a);

    scene->image[offset] += (pixel.r * alpha);
    scene->image[offset + 1] += (pixel.g * alpha);
    scene->image[offset + 2] += (pixel.b * alpha);
    scene->image[offset + 3] = pixel.a;
}


/**
 * @brief fill in a rectangle from x1,y1 to x2,y2. x2,y2 do not need to be > x1,y1
 * 
 * @param scene 
 * @param x1
 * @param y1
 * @param x2 
 * @param y2 
 * @param color 
 */
void hub_fill(scene_info *scene, const uint16_t x1, const uint16_t y1, const uint16_t x2, const uint16_t y2, const RGB color) {
    uint16_t fx1 = x1 % scene->width-1;
    uint16_t fx2 = x2 % scene->width-1;
    uint16_t fy1 = y1 % scene->height-1;
    uint16_t fy2 = y2 % scene->height-1;

    if (fx2 < fx1) {
        uint16_t temp = fx1;
        fx1 = fx2;
        fx2 = temp;
    }
    if (fy2 < fy1) {
        uint16_t temp = fy1;
        fy1 = fy2;
        fy2 = temp;
    }
    for (int y = fy1; y < fy2; y++) {
        for (int x = fx1; x < fx2; x++) {
            hub_pixel(scene, x, y, color);
        }
    }
}

/**
 * @brief fill in a rectangle of width,height at x,y with the specified color
 * 
 * @param scene 
 * @param x 
 * @param y 
 * @param width 
 * @param height 
 * @param color 
 */
void hub_fill_grad(scene_info *scene, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, Gradient gradient) {
    if (x1 < x0) {
        uint16_t temp = x0;
        x0 = x1;
        x1 = temp;
    }
    if (y1 < y0) {
        uint16_t temp = y0;
        y0 = y1;
        y1 = temp;
    }
    ASSERT(y1 < scene->height);
    ASSERT(x1 < scene->width);
    printf("%dx%d, %dx%d\n", x0, y0, x1, y1);


    RGB left, right, final;
    float h_ratio, v_ratio = 0.0f;
    for (int y = y0; y < y1; y++) {
        v_ratio = (float)(y - y0) / (y1 - y0);

        //float vertical = gradient.type(y0, y1, x0, x1, v_ratio, 0);
        float vertical = gradient.type(x0, y0, x1, y1, v_ratio, 0);
        interpolate_rgb(&left, gradient.colorA1, gradient.colorA2, vertical);
        interpolate_rgb(&right, gradient.colorB1, gradient.colorB2, vertical);

        for (int x = x0; x < x1; x++) {
            h_ratio = (float)(x - x0) / (x1 - x0);

            //float horizontal = gradient.type(y0, y1, x0, x1, v_ratio, h_ratio);
            float horizontal = gradient.type(y0, y1, x0, x1, v_ratio, h_ratio);
            printf("v: %f, h: %f\n", vertical, horizontal);
            interpolate_rgb(&final, left, right, horizontal);

            hub_pixel(scene, x, y, final);
        }
    }
}


// Draw an unfilled circle using Bresenham's algorithm
void hub_circle(scene_info *scene, const uint16_t width, const uint16_t height, const uint16_t centerX, const uint16_t centerY, const uint16_t radius, const RGB color) {
    int x = radius;
    int y = 0;
    int decisionOver2 = 1 - x; // Decision variable


    while (x >= y) {
        hub_pixel(scene, centerX + x, centerY + y, color);
        hub_pixel(scene, centerX + y, centerY + x, color);
        hub_pixel(scene, centerX - y, centerY + x, color);
        hub_pixel(scene, centerX - x, centerY + y, color);

        hub_pixel(scene, centerX - x, centerY - y, color);
        hub_pixel(scene, centerX - y, centerY - x, color);
        hub_pixel(scene, centerX + y, centerY - x, color);
        hub_pixel(scene, centerX + x, centerY - y, color);
        
        y++;

        // Update decision variable
        if (decisionOver2 <= 0) {
            decisionOver2 += 2 * y + 1; // East
        } else {
            x--;
            decisionOver2 += 2 * (y - x) + 1; // Southeast
        }
    }
}


/**
 * @brief draw a line using Bresenham's line drawing algorithm
 * 
 * @param scene 
 * @param x0 start pixel x location
 * @param y0 start pixel y location
 * @param x1 end pixel x
 * @param y1 end pixel y
 * @param color color to draw the line
 */
void hub_line(scene_info *scene, int x0, int y0, int x1, int y1, RGB color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1; // Step in the x direction
    int sy = (y0 < y1) ? 1 : -1; // Step in the y direction
    int err = dx - dy;           // Error value

    while (1) {
        hub_pixel(scene, x0, y0, color); // Set pixel

        // Check if we've reached the end point
        if (x0 == x1 && y0 == y1) break;

        int err2 = err * 2;
        if (err2 > -dy) { // Error term for the x direction
            err -= dy;
            x0 += sx;
        }
        if (err2 < dx) { // Error term for the y direction
            err += dx;
            y0 += sy;
        }
    }
}


/**
 * @brief draw an anti-aliased line using Xiolin Wu's anti-aliased line drawing algorithm
 * 
 * @param scene 
 * @param x0 start pixel x location
 * @param y0 start pixel y location
 * @param x1 end pixel x
 * @param y1 end pixel y
 * @param color color to draw the line
 */
void hub_line_aa(scene_info *scene, const int x0, const int y0, const int x1, const int y1, const RGB color) {

    int fx0 = MIN(x0, scene->width-1);
    int fx1 = MIN(x1, scene->width-1);
    int fy0 = MIN(y0, scene->height-1);
    int fy1 = MIN(y1, scene->height-1);

    float dx = (float)(fx1 - fx0);
    float dy = (float)(fy1 - fy0);
    
    int steep = fabs(dy) > fabs(dx);
    
    if (steep) {
        // Swap x and y
        int tmp;
        tmp = fx0; fx0 = fy0; fy0 = tmp;
        tmp = fx1; fx1 = fy1; fy1 = tmp;
        dx = (float)(fx1 - fx0);
        dy = (float)(fy1 - fy0);
    }
    
    if (fx0 > fx1) {
        // Swap (fx0, fy0) with (fx1, fy1)
        int tmp;
        tmp = fx0; fx0 = fx1; fx1 = tmp;
        tmp = fy0; fy0 = fy1; fy1 = tmp;
        dx = (float)(fx1 - fx0);
        dy = (float)(fy1 - fy0);
    }

    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    // Handle the first endpoint
    float xend = roundf(fx0);
    float yend = fy0 + gradient * (xend - fx0);
    float xgap = rfpart(fx0 + 0.5f);
    int xpxl1 = (int)xend;
    int ypxl1 = ipart(yend);
    if (steep) {
        hub_pixel_factor(scene, ypxl1, xpxl1, color, rfpart(yend) * xgap);
        hub_pixel_factor(scene, ypxl1 + 1, xpxl1, color, fpart(yend) * xgap);
    } else {
        hub_pixel_factor(scene, xpxl1, ypxl1, color, rfpart(yend) * xgap);
        hub_pixel_factor(scene, xpxl1, ypxl1 + 1, color, fpart(yend) * xgap);
    }
    float intery = yend + gradient;  // First y-intersection for the main loop

    // Handle the second endpoint
    xend = roundf(fx1);
    yend = fy1 + gradient * (xend - fx1);
    xgap = fpart(fx1 + 0.5f);
    int xpxl2 = (int)xend;
    int ypxl2 = ipart(yend);
    if (steep) {
        hub_pixel_factor(scene, ypxl2, xpxl2, color, rfpart(yend) * xgap);
        hub_pixel_factor(scene, ypxl2 + 1, xpxl2, color, fpart(yend) * xgap);
    } else {
        hub_pixel_factor(scene, xpxl2, ypxl2, color, rfpart(yend) * xgap);
        hub_pixel_factor(scene, xpxl2, ypxl2 + 1, color, fpart(yend) * xgap);
    }

    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            hub_pixel_factor(scene, ipart(intery), x, color, rfpart(intery));
            hub_pixel_factor(scene, ipart(intery) + 1, x, color, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            hub_pixel_factor(scene, x, ipart(intery), color, rfpart(intery));
            hub_pixel_factor(scene, x, ipart(intery) + 1, color, fpart(intery));
            intery += gradient;
        }
    }
}


/**
 * @brief  draw an un-anti aliased triangle
 * 
 * @param scene 
 * @param x0 
 * @param y0 
 * @param x1 
 * @param y1 
 * @param x2 
 * @param y2 
 * @param color 
 */
void hub_triangle(scene_info *scene, int x0, int y0, int x1, int y1, int x2, int y2, RGB color) {
    hub_line(scene, x0, y0, x1, y1, color);
    hub_line(scene, x1, y1, x2, y2, color);
    hub_line(scene, x2, y2, x0, y0, color);
}

void hub_triangle_aa(scene_info *scene, int x0, int y0, int x1, int y1, int x2, int y2, RGB color) {
    hub_line_aa(scene, x0, y0, x1, y1, color);
    hub_line_aa(scene, x1, y1, x2, y2, color);
    hub_line_aa(scene, x2, y2, x0, y0, color);
}

#include <unistd.h>
#include <stdint.h>

#include "rpihub75.h"

#ifndef _UTIL_H
#define _UTIL_H 1

/**
 * @brief used to set the pin mode of a GPIO pin using mmaped /dev/gpiomem0 
 * 
 */
typedef struct{
    uint32_t status;
    uint32_t ctrl; 
}GPIOregs;
#define GPIO ((GPIOregs*)GPIOBase)

/**
 * @brief helper struct for accessing the RIO registers of a GPIO pin
 * 
 */
typedef struct
{
    uint32_t Out;
    uint32_t OE;
    uint32_t In;
    uint32_t InSync;
} rioregs;


/**
 * @brief printf() a message to stderr and exit with a non-zero status
 * 
 * @param message 
 * @param ... 
 */
void die(const char *message, ...);

/**
 * display a message to stderr if CONSOLE_DEBUG is defined 
 */
void debug(const char *format, ...);
uint32_t *create_jitter_mask(uint16_t jitter_size, uint8_t brightness);
int file_put_contents(const char *filename, const void *data, size_t size);
void binary32(FILE *fd, const uint32_t number);
void binary64(FILE *fd, const uint64_t number);
int rnd(unsigned char *buffer, size_t size);
uint32_t* map_gpio(uint32_t offset);
void configure_gpio(uint32_t *PERIBase);
char *file_get_contents(const char *filename, long *filesize);

// create a default scene configuration.  caller must free() the scene
scene_info *default_scene(int argc, char **argv);
uint8_t *u_mapper_impl(const uint8_t *image_in, uint8_t *image_out, const struct scene_info *scene);
void *calibrate_panels(void *arg);

#endif

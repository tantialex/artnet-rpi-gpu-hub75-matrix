#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "util.h"
#include "rpihub75.h"


extern char *optarg;
uint16_t global_width = 0;

/**
 * @brief die with a message
 * 
 * @param message 
 */
void die(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    exit(1);
}

/**
 * @brief a wrapper around fprintf(stderr, ...) that also prints the current time
 * accepts var args
 * 
 * @param ... 
 * @return void 
 */
void debug(const char *format, ...) {
    if (CONSOLE_DEBUG) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}



/**
 * @brief write data to a file
 * 
 * @param filename filename to write to (wb)
 * @param data data to write
 * @param size number of bytes to write
 * @return int number of bytes written, -1 on error
 */
int file_put_contents(const char *filename, const void *data, size_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        return -1;
    }
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    return written;
}

/**
 * @brief read in a file, allocate memory and return the data. caller must free.
 * this function will set filesize, you do not need to pass in filesize.
 * 
 * @param filename - file to read
 * @param filesize - pointer to the size of the file. will be set after the call. ugly i know
 * @return char* - pointer to read data. NOTE: caller must free
 */
char *file_get_contents(const char *filename, long *filesize) {
    FILE *file = fopen(filename, "rb");  // Open the file in binary mode
    if (file == NULL) {
        perror("Could not open file");
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    *filesize = ftell(file);
    rewind(file);  // Go back to the beginning of the file

    // Allocate memory for the file contents + null terminator
    char *buffer = (char *)malloc(*filesize + 1);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    // Read the file into the buffer
    size_t read_size = fread(buffer, 1, *filesize, file);
    if (read_size != *filesize) {
        perror("Failed to read the complete file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    // Null-terminate the string
    buffer[*filesize] = '\0';

    fclose(file);
    return buffer;
}

// Helper function to print a number in binary format
void binary32(FILE *fd, const uint32_t number) {
    // Print the number in binary format, ensure it only shows 11 bits
    for (int i = 31; i >= 0; i--) {
        fprintf(fd, "%d", (number >> i) & 1);
    }
}


// Helper function to print a number in binary format
void binary64(FILE *fd, const uint64_t number) {
    // Print the number in binary format, ensure it only shows 11 bits
    for (int i = 63; i >= 0; i--) {
        fprintf(fd, "%ld", (number >> i) & 1);
    }
}

int rnd(unsigned char *buffer, size_t size) {
    // Open /dev/urandom for strong random data
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/urandom");
        return -1;
    }

    // Read 'size' bytes from /dev/urandom into the buffer
    ssize_t bytes_read = read(fd, buffer, size);
    if (bytes_read != size) {
        perror("Failed to read random data");
        close(fd);
        exit(-1);
    }

    // Close the file descriptor
    close(fd);
    return 0;
}

/**
 * @brief  calculate a jitter mask for the OE pin that should randomly toggle the OE pin on/off acording to brightness
 * TODO: look for rows of > 3 bits that are all the same and spread these bits out. this will reduce flicker on the display
 * 
 * @param jitter_size  prime number > 1024 < 4096
 * @param brightness   larger values produce brighter output, max 255
 * @return uint32_t*   a pointer to the jitter mask. caller must release memory
 */
uint32_t *create_jitter_mask(uint16_t jitter_size, uint8_t brightness) {
    srand(time(NULL));
    uint32_t *jitter = (uint32_t*)malloc(jitter_size*sizeof(uint32_t));
    uint8_t *raw_crypto = (uint8_t*)malloc(jitter_size);
    memset(jitter, 0, jitter_size*sizeof(uint32_t));
    // read random data from urandom into the raw_crypto buffer
    rnd(raw_crypto, jitter_size);
    // map raw crypto data to the global OE jitter mask (toggle the OE pin on/off for JITTERS_SIZE frames)
    for (int i=0; i<jitter_size; i++) {
        if (raw_crypto[i] > brightness) {
            jitter[i] = PIN_OE;
        }
    }

    //return jitter;

    // make 5 passes at long run reduction ....
    for (int j = 0; j < 3; j++) {
        // Detect and redistribute runs of >4 identical bits
        for (int i = 0; i < jitter_size - 4; i++) {
            int run_length = 1;

            // Check if we have a run of similar bits (either all set or all clear)
            while (i + run_length < jitter_size && jitter[i] == jitter[i + run_length]) {
                run_length++;
            }

            // If the run length is more than 3, recalculate the bits
            if (run_length > 3) {
                // recreate these bits
                for (int j = i; j < i + run_length; j++) {
                    jitter[j] = ((rand() % 255) > brightness) ? PIN_OE : 0;  // Randomly set or clear the OE pin
                }
            }

            // Skip over the run we just processed
            i += run_length - 1;
        }
    }

    free(raw_crypto);
    return jitter;
}


void calculate_fps(long sleep_time) {
    // Variables to track FPS
    static int frame_count = 0;
    static time_t last_time = 0;
    //static int fps = 0;
    time_t current_time = time(NULL);

    // If one second has passed
    if (current_time != last_time) {
        // Output FPS
        printf("FPS: %d (%ld)\n", frame_count, sleep_time);

        // Reset frame count and update last_time
        // fps = frame_count;
        frame_count = 0;
        last_time = current_time;
    }

    // Increment the frame counter
    frame_count++;
}


uint32_t* map_gpio(uint32_t offset) {

    int mem_fd = open("/dev/gpiomem0", O_RDWR | O_SYNC);
    uint32_t *map = (uint32_t *)mmap(
        NULL,
        64 * 1024 * 1024,
        (PROT_READ | PROT_WRITE),
        MAP_SHARED,
        mem_fd,
        PERI_BASE
    );
        //0x1f00000000 (on pi5 for root access to /dev/mem use this address)
    if (map == MAP_FAILED)
    {
        printf("mmap failed: %s [%ld] / [%lx]\n", strerror(errno), PERI_BASE, PERI_BASE);
        exit (-4);
    };
    close(mem_fd);

    return map;
}


// Function to get a character without needing Enter
char getch() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);           // Get current terminal attributes
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);         // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Set new attributes
    ch = getchar();                           // Get the character
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore old attributes
    return ch;
}
 

 void configure_gpio(uint32_t *PERIBase) {
    // set all GPIO pins to output mode, fast slew rate, pull down enabled
    // https://www.i-programmer.info/programming/148-hardware/16887-raspberry-pi-iot-in-c-pi-5-memory-mapped-gpio.html

    for (uint32_t pin_num=2; pin_num<28; pin_num++) {
        uint32_t fn = 5;
        uint32_t *PADBase  = PERIBase + PAD_OFFSET;
        uint32_t *pad = PADBase + 1;   
        uint32_t *GPIOBase = PERIBase + GPIO_OFFSET;
        uint32_t *RIOBase = PERIBase + RIO_OFFSET;
	    GPIO[pin_num].ctrl = fn;
	    pad[pin_num] = 0x15;

        // these seem to be required. todo: test removing them
        rioSET->OE = 0x01<<pin_num;
        rioSET->Out = 0x01<<pin_num;
        debug("configured pin [%d]\n", pin_num);
    }
}

void usage(int argc, char **argv) {
    die(
        "Usage 2: %s\n"
        "     -s <shader file>  GPU fragment shader file to render\n"
        "     -x <width>        image width              (16-384)\n"
        "     -y <height>       image height             (16-384)\n"
        "     -w <width>        panel width              (16/32/64)\n"
        "     -h <height>       panel height             (16/32/64)\n"
        "     -f <fps>          frames per second        (1-255)\n"
        "     -p <num ports>    number of ports          (1-3)\n"
        "     -c <num chains>   number of chains         (1-16)\n"
        "     -g <gamma>        gamma correction         (1.0-2.8)\n"
        "     -d <bit depth>    bit depth                (2-32)\n"
        "     -b <brightness>   overall brightness level (0-255)\n"
        "     -j                disable jitter, adjust PWM brightness instead\n"
        "     -l <frames>       motion blur frames       (0-32)\n"
        "     -v                vertical mirror image\n"
        "     -m                mirror output image\n"
        "     -i <mapper>       image mapper (u)\n"
        "     -t <tone_mapper>  (aces, reinhard, hable)\n"
        "     -z                run LED calibration script\n"
        "     -n                display data from UDP server on port %d\n"
        "     -h                this help\n", argv[0], SERVER_PORT);
}


scene_info *default_scene(int argc, char **argv) {
    // setup all scene configuration info
    scene_info *scene = (scene_info*)malloc(sizeof(scene_info));
    memset(scene, 0, sizeof(scene_info));
    scene->width = IMG_WIDTH;
    scene->height = IMG_HEIGHT;
    scene->panel_height = PANEL_HEIGHT;
    scene->panel_width = PANEL_WIDTH;
    scene->num_chains = 4;
    scene->num_ports = 1;
    scene->buffer_ptr = 0;
    scene->stride = 4;
    scene->gamma = GAMMA;
    scene->red_gamma = RED_GAMMA_SCALE;
    scene->green_gamma = GREEN_GAMMA_SCALE;
    scene->blue_gamma =BLUE_GAMMA_SCALE; 
    scene->red_linear = RED_SCALE;
    scene->green_linear = GREEN_SCALE;
    scene->blue_linear = BLUE_SCALE;
    scene->jitter_brightness = true;

    scene->bit_depth = 32;
    scene->pwm_mapper = map_byte_image_to_pwm;
    scene->tone_mapper = copy_tone_mapperF;
    scene->brightness = 200;
    scene->motion_blur_frames = 0;

    // calculate max fps for this configuration
    scene->fps = 9600 / scene->bit_depth / (scene->panel_width / 16);

    // print usage if no arguments
    if (argc < 2) { 
        usage(argc, argv);
    }

    // Parse command-line options
    int opt;
    while ((opt = getopt(argc, argv, "x:y:w:h:s:f:p:c:g:d:b:it:ml:i:vjz?")) != -1) {
        switch (opt) {
        case 's':
            scene->shader_file = optarg;
            break;
        case 'x':
            scene->width = atoi(optarg);
            global_width = scene->width;
            break;
        case 'y':
            scene->height = atoi(optarg);
            break;
        case 'w':
            scene->panel_width = atoi(optarg);
            break;
        case 'h':
        case '?':
            scene->panel_height = atoi(optarg);
            if (scene->panel_height <= 1) {
                usage(argc, argv);
            }
            break;
        case 'f':
            scene->fps = atoi(optarg);
            break;
        case 'p':
            scene->num_ports = atoi(optarg);
            break;
        case 'c':
            scene->num_chains = atoi(optarg);
            break;
        case 'g':
            scene->gamma = atof(optarg);
            break;
        case 'd':
            scene->bit_depth = atoi(optarg);
            break;
        case 'b':
            scene->brightness = atoi(optarg);
            break;
        case 'v':
            scene->invert = true;
            break;
        case 'm':
            scene->image_mirror = true;
            break;
        case 'l':
            scene->motion_blur_frames = atoi(optarg);
            break;
        case 'j':
            scene->jitter_brightness = false;
            break;
        case 'z':
            scene->gamma = -99.0f;
            break;
        case 't':
            if (strcmp(optarg, "aces") == 0) {
                scene->tone_mapper = aces_tone_mapperF;
            } else if (strcmp(optarg, "reinhard") == 0) {
                scene->tone_mapper = reinhard_tone_mapperF;
            } else if (strcmp(optarg, "hable") == 0) {
                scene->tone_mapper = hable_tone_mapperF;
            } else if (strcmp(optarg, "none") == 0) {
                scene->tone_mapper = copy_tone_mapperF;
            } else {
                die("Unknown tone mapper: %s, must be one of (aces, reinhard, hable, none)\n", optarg);
            }
            break;
        case 'i':
            if (strcasecmp(optarg, "u") == 0) {
                debug("set U mapper\n");
                scene->image_mapper = u_mapper_impl;
            } else {
                die("Unknown tone mapper: %s, must be one of (aces, reinhard, hable, none)\n", optarg);
            }
            break;
        default:
            usage(argc, argv);
        }
    }

    size_t buffer_size = (scene->width + 1) * (scene->height + 1) * 3 * scene->bit_depth;
    // force the buffers to be 16 byte aligned to improve auto vectorization
    scene->pwm_signalA = aligned_alloc(16, buffer_size * 4);
    scene->image = aligned_alloc(16, scene->width * scene->height * 4); // make sure we always have enough for RGBA

    return scene;
}


void *calibrate_panels(void *arg) {
    scene_info *scene = (scene_info*)arg;
    printf("Point your browser to: file:///home/cory/mnt/calibration.html\n");
    printf("After calibrating each set of vertical bar.  press any key on your browser window to continue\n\n");

    printf("a - lower gamma,          A - raise gamma\n");
    printf("r - lower red linearly,   R - raise red linearly\n");
    printf("g - lower green linearly, G - raise green linearly\n");
    printf("b - lower blue linearly,  B - raise blue linearly\n");
    printf("t - lower red gamma,      T - raise red gamma\n");
    printf("h - lower green gamma,    H - raise green gamma\n");
    printf("n - lower blue gamma,     N - raise blue gamma\n");
    printf("<enter> next color\n");
	float rgb_bars[12][3] = {
        {1.0, 1.0, 1.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
        {1.0, 1.0, 1.0},
        {0.0, 0.4, 0.8},
        {0.8, 0.4, 1.0},
        {0.8, 0.0, 0.4},
        {0.0, 0.8, 0.4},
        {0.4, 0.8, 0.0},
        {0.4, 0.0, 0.8},
        {0.4, 0.6, 0.8}
    };

    printf("created rgb bars\n");

    RGB c1;/* = {0x04, 0x20, 0x7c};
    RGB c2 = {0x7c, 0x96, 0x09};
    RGB c3 = {0x9e, 0x01, 0x38};
    RGB c4 = {0xed, 0x12, 0xb2};
    RGB c5 = {0xce, 0x9e, 0x00};
    */

    // const scene_info *scene = (scene_info*)arg;
    uint8_t *image          = (uint8_t*)malloc(scene->width * scene->height * scene->stride);
    memset(image, 0, scene->width * scene->height * scene->stride);
    uint8_t j = 0, num_col = 5;
    printf("malloc memory %dx%d\n", scene->width, scene->height);

    while (j < 12) {

        for (int y = 0; y < scene->height; y++) {
            for (int x=0; x<scene->panel_width; x++) {
                RGB *pixel = (RGB *)(image + ((y * scene->panel_width) + x) * scene->stride);
                //printf("pixel ...%dx%d\n", x, y);
                int i = x / floor(scene->panel_width / num_col);
                //if (y == 0) { printf("x = %d, i = %d = v:%d\n", x, i, (254 / (i+1))); }
                c1.r = rgb_bars[j][0] * (254 / (i+1));
                c1.g = rgb_bars[j][1] * (254 / (i+1));
                c1.b = rgb_bars[j][2] * (254 / (i+1));
                *pixel = c1;
            }
        }

        map_byte_image_to_pwm(image, scene, TRUE);
        char ch = getch();
        if (ch == 'a') {
            scene->gamma -= 0.01f;
            printf("gamma        down = %f\n", (double)scene->gamma);
        } else if (ch == 'A') {
            scene->gamma += 0.01f;
            printf("gamma        up   = %f\n", (double)scene->gamma);
        } else if (ch == 'g') {
            scene->green_linear -= 0.01f;
            printf("green_linear down = %f\n", (double)scene->green_linear);
        } else if (ch == 'G') {
            scene->green_linear += 0.01f;
            printf("green_linear up   = %f\n", (double)scene->green_linear);
        } else if (ch == 'h') {
            scene->green_gamma -= 0.01f;
            printf("green_gamma down  = %f\n", (double)scene->green_gamma);
        } else if (ch == 'H') {
            scene->green_gamma += 0.01f;
            printf("green_gamma up    = %f\n", (double)scene->green_gamma);
        } else if (ch == 't') {
            scene->red_gamma -= 0.01f;
            printf("red_gamma down    = %f\n", (double)scene->red_gamma);
        } else if (ch == 'T') {
            scene->red_gamma += 0.01f;
            printf("red_gamma up      = %f\n", (double)scene->red_gamma);
        } 
         else if (ch == 'n') {
            scene->blue_gamma -= 0.01f;
            printf("blue_gamma down   = %f\n", (double)scene->blue_gamma);
        } else if (ch == 'N') {
            scene->blue_gamma += 0.01f;
            printf("blue_gamma up     = %f\n", (double)scene->blue_gamma);
        } 
         else if (ch == 'b') {
            scene->blue_linear -= 0.01f;
            printf("blue_linear down  = %f\n", (double)scene->blue_linear);
        } else if (ch == 'B') {
            scene->blue_linear += 0.01f;
            printf("blue_linear up    = %f\n", (double)scene->blue_linear);
        } else if (ch == 'R') {
            scene->red_linear += 0.01f;
            printf("red_linear up     = %f\n", (double)scene->red_linear);
        } else if (ch == 'r') {
            scene->red_linear -= 0.01f;
            printf("red_linear down   = %f\n", (double)scene->red_linear);
        }
        else if (ch == '\n') {
            // show all values
            printf("gamma: %f, red_linear: %f, green_linear: %f, blue_linear: %f\n", (double)scene->gamma, (double)scene->red_linear, (double)scene->green_linear, (double)scene->blue_linear);
            // show lal gamma values
            printf("red_gamma: %f, green_gamma: %f, blue_gamma: %f\n", (double)scene->red_gamma, (double)scene->green_gamma, (double)scene->blue_gamma);
            j++;
            if (j >= 12) {
                die("calibration complete\n");
            }
        }
        scene->tone_mapper = (scene->tone_mapper == NULL) ? copy_tone_mapperF : NULL;
    }

    return NULL;
}



// Function to receive UDP packets and reassemble the frame
void* receive_udp_data(void *arg) {
    const scene_info *scene = (scene_info *)arg; // dereference the scene info
    int sock;
    struct sockaddr_in server_addr;
    struct udp_packet packet;
    uint16_t max_frame_sz  = scene->width * scene->height * scene->stride;
    uint16_t max_packet_id = ceilf((float)max_frame_sz / (float)(PACKET_SIZE - 10));

    uint8_t *image_data = malloc(max_frame_sz * 16);

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        die("Server socket creation failed\n");
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(sock, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        die("Bind failed");
    }

    for(;;) {
        socklen_t len = sizeof(server_addr);
        int n = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&server_addr, &len);
        if (n < 0) {
            close(sock);
            die("Receive failed");
        }

        // Check preamble for data alignment
        if (ntohl(packet.preamble) != PREAMBLE) {
            printf("Invalid preamble received\n");
            continue;
        }

        const uint16_t packet_id = ntohs(packet.packet_id);
        const uint16_t total_packets = ntohs(packet.total_packets);


        const uint16_t frame_num = ntohs(packet.frame_num) % 8;
        const uint16_t frame_off = MIN(MIN(packet_id, max_packet_id) * PACKET_SIZE, max_frame_sz-1);

        memcpy(image_data + ((frame_num * max_frame_sz) + frame_off), packet.data, PACKET_SIZE - 10);
        if (packet_id == total_packets) {
            // map to pwm data
            scene->pwm_mapper(image_data + (frame_num * max_frame_sz), scene, TRUE);
        }

    }

    close(sock);
}

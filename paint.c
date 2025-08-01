#include <fcntl.h>
#include <getopt.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#include "include/scale_img.h"

uint32_t image_width, image_height;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
struct termios oldt, newt;
char *filename;
volatile sig_atomic_t interrupt = 0;
int fb_fd;
char *fb_ptr;
char brush_color[3] = {0xFF, 0xFF, 0xFF}; // Default brush color: white
int brush_size = 30; // Default brush size

void draw_circle(int x, int y) {
    int r = brush_size / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                int px = x + dx;
                int py = y + dy;
                if (px >= (vinfo.xres - image_width) / 2 && px < vinfo.xres - (vinfo.xres - image_width) / 2 - 1 && py >= (vinfo.yres - image_height) / 2 &&
                    py < vinfo.yres - (vinfo.yres - image_height) / 2 - 1) {
                    int offset = py * finfo.line_length + px * (vinfo.bits_per_pixel / 8);
                    if (offset >= 0 && offset < finfo.smem_len) {
                        *((char *)(fb_ptr + offset + (vinfo.red.offset / 8))) = brush_color[0];
                        *((char *)(fb_ptr + offset + (vinfo.green.offset / 8))) = brush_color[1];
                        *((char *)(fb_ptr + offset + (vinfo.blue.offset / 8))) = brush_color[2];
                    }
                }
            }
        }
    }
}

int **bresenham(int x1, int y1, int x2, int y2, int *out_count) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int max_points = dx > dy ? dx + 1 : dy + 1; // number of points on the line
    int **points = malloc(max_points * sizeof(int *));
    if (!points) return NULL;

    for (int i = 0; i < max_points; i++) {
        points[i] = malloc(2 * sizeof(int));
        if (!points[i]) {
            // free previously allocated rows on failure
            for (int j = 0; j < i; j++) free(points[j]);
            free(points);
            return NULL;
        }
    }

    int err = (dx > dy ? dx : -dy) / 2;
    int x = x1, y = y1, i = 0;

    while (true) {
        points[i][0] = x;
        points[i][1] = y;
        i++;

        if (x == x2 && y == y2) break;

        int e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x += sx;
        }
        if (e2 < dy) {
            err += dx;
            y += sy;
        }
    }

    *out_count = i;
    return points;
}

void parse_color(const char *buffer, char *brush_color) {
    // Skip '#' or '0x' if present
    if (buffer[0] == '#')
        buffer++;
    else if (strncmp(buffer, "0x", 2) == 0)
        buffer += 2;

    unsigned int hex;
    if (sscanf(buffer, "%06x", &hex) == 1) {
        brush_color[0] = (hex >> 16) & 0xFF; // Red
        brush_color[1] = (hex >> 8) & 0xFF; // Green
        brush_color[2] = hex & 0xFF; // Blue
    }
}

void sigint() {
    interrupt = 1;
}

void save_and_exit() {
    char *data = malloc(image_width * image_height * 3);
    for (int i = 0; i < image_height; i++) {
        for (int v = 0; v < image_width; v++) {
            int offset =
                (i + (vinfo.yres - image_height) / 2) * finfo.line_length +
                (v + (vinfo.xres - image_width) / 2) *
                    (vinfo.bits_per_pixel / 8);
            int data_offset = (i * image_width + v) * 3;
            char red = *((char *)(fb_ptr + offset + (vinfo.red.offset / 8)));
            char green = *((char *)(fb_ptr + offset + (vinfo.green.offset / 8)));
            char blue = *((char *)(fb_ptr + offset + (vinfo.blue.offset / 8)));
            data[data_offset] = red;
            data[data_offset + 1] = green;
            data[data_offset + 2] = blue;
        }
    }
    FILE *file = fopen(filename, "wb");
    tcsetattr(STDOUT_FILENO, TCSANOW, &oldt);
    if (!file) {
        perror("Error opening file for writing");
        free(data);
        exit(EXIT_FAILURE);
    }
    fwrite("FBIMG", 1, 5, file);
    fwrite(&image_width, sizeof(uint32_t), 1, file);
    fwrite(&image_height, sizeof(uint32_t), 1, file);
    char color[4] = "RGB"; // Default color format
    fwrite(color, 1, 3, file);
    fwrite(data, 1, image_width * image_height * 3, file);
    fclose(file);
    printf("\033[?25h\033[H\033[J"); // Show cursor and clear console
    fflush(stdout);
    munmap(fb_ptr, finfo.smem_len);
    close(fb_fd);
    free(data);
    exit(0);
}

int draw_image(char fname[], int offset_x, int offset_y, bool centered) {
    filename = malloc(strlen(fname) + 1);
    strcpy(filename, fname);
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }
    char magic[6] = {0}; // Allocate space for 5 characters + null terminator
    size_t bytesRead = fread(magic, 1, 5, file);
    if (bytesRead != 5) {
        fprintf(stderr, "Unexpected end of file\n");
        fclose(file);
        return 1;
    }
    if (strcmp(magic, "FBIMG") != 0) {
        fprintf(stderr, "Not a valid FBIMG file\n");
        fclose(file);
        return 1;
    }
    fread(&image_width, sizeof(uint32_t), 1, file);
    fread(&image_height, sizeof(uint32_t), 1, file);
    char color[4] = {0};
    fread(color, 1, 3, file);
    char *data = malloc(image_width * image_height * 3);
    fread(data, 1, image_width * image_height * 3, file);
    fclose(file);

    if (vinfo.red.length != 8 || vinfo.green.length != 8 ||
        vinfo.blue.length != 8) {
        fprintf(stderr,
                "Error: framebuffer color depth per channel is not 8 bits; "
                "framebuffer not supported\n");
        close(fb_fd);
        return 1;
    }

    if (vinfo.xres < image_width) {
        data = scale_image(data, false, image_width, image_height, vinfo.xres,
                           image_height * ((float)vinfo.xres / image_width));
        image_width = vinfo.xres;
        image_height = (int)(image_height * ((float)vinfo.xres / image_width));
    }

    if (vinfo.yres < image_height) {
        data = scale_image(data, false, image_width, image_height,
                           image_width * ((float)vinfo.yres / image_height),
                           vinfo.yres);
        image_width = (int)(image_width * ((float)vinfo.yres / image_height));
        image_height = vinfo.yres;
    }

    // Map framebuffer memory
    size_t screensize = finfo.smem_len;
    fb_ptr =
        mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        perror("Error mapping framebuffer memory");
        close(fb_fd);
        return 1;
    }

    if (centered) {
        offset_x = (vinfo.xres - image_width) / 2;
        offset_y = (vinfo.yres - image_height) / 2;
    }

    // Write pixels to framebuffer
    for (int i = 0; i < image_height; i++) {
        for (int j = 0; j < image_width; j++) {
            int offset = (i + offset_y) * finfo.line_length +
                         (j + offset_x) * (vinfo.bits_per_pixel / 8);
            int data_offset = (i * image_width + j) * 3;
            char red;
            char green;
            char blue;
            char alpha = 0xFF;
            if (strcmp(color, "RGB") == 0) {
                red = data[data_offset];
                green = data[data_offset + 1];
                blue = data[data_offset + 2];
            } else if (strcmp(color, "BGR") == 0) {
                blue = data[data_offset];
                green = data[data_offset + 1];
                red = data[data_offset + 2];
            }
            if (vinfo.transp.length > 0) {
                *((char *)(fb_ptr + offset + (vinfo.transp.offset / 8))) =
                    alpha;
            }
            *((char *)(fb_ptr + offset + (vinfo.red.offset / 8))) = red;
            *((char *)(fb_ptr + offset + (vinfo.green.offset / 8))) = green;
            *((char *)(fb_ptr + offset + (vinfo.blue.offset / 8))) = blue;
        }
    }

    free(data);
    return 0;
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"usage", no_argument, NULL, 'u'},
        {"color", required_argument, NULL, 'c'},
        {"size", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, "huc:s:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [options] [filename]\n", argv[0]);
                printf("Options:\n");
                printf("  -h, --help     Show this help message\n");
                printf("  -u, --usage    Show usage information\n");
                printf("  -c, --color    Set brush color (hex format)\n");
                printf("  -s, --size     Set brush size (positive integer)\n");
                return 0;
            case 'u':
                printf("A painting program that runs on the framebuffer\n");
                return 0;
            case 'c':
                parse_color(optarg, brush_color);
                break;
            case 's':
                brush_size = atoi(optarg);
                if (brush_size <= 0) {
                    fprintf(stderr, "Error: Brush size must be a positive integer\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [options] [filename]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("\033[?25l"); // Hide cursor
    fflush(stdout);
    signal(SIGINT, sigint);
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO); // Disable echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        return 1;
    }

    // Get variable screen info
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error getting screen info");
        close(fb_fd);
        return 1;
    }

    // Get fixed screen info
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error getting fixed screen info");
        close(fb_fd);
        return 1;
    }
    if (vinfo.red.length != 8 || vinfo.green.length != 8 ||
        vinfo.blue.length != 8) {
        fprintf(stderr,
                "Error: framebuffer color depth per channel is not 8 bits; "
                "framebuffer not supported\n");
        close(fb_fd);
        return 1;
    }
    if (optind < argc) {
        char *filename = argv[optind];
        draw_image(filename, 0, 0, true);
    } else {
        fb_ptr = mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        for (int i = 0; i < vinfo.yres * finfo.line_length; i++) {
            *((char *)(fb_ptr + i)) = 0; // Clear framebuffer
        }
        image_width = vinfo.xres;
        image_height = vinfo.yres;
        filename = "paint.fbimg";
    }
    int mousedev = open("/dev/input/mice", O_RDONLY);
    if (mousedev == -1) {
        perror("Error opening mouse device");
        munmap(fb_ptr, finfo.smem_len);
        close(fb_fd);
        return 1;
    }
    int x = 0, y = 0;
    int prev_x = 0, prev_y = 0;
    char *pixel = (char[]){fb_ptr[0], fb_ptr[1], fb_ptr[2]};
    while (true) {
        if (interrupt) {
            save_and_exit();
        }
        char *buffer = malloc(256);
        ssize_t bytes_read = read(STDIN_FILENO, buffer, 256);
        if (bytes_read != -1) {
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "dq\n") == 0) {
                tcsetattr(STDOUT_FILENO, TCSANOW, &oldt);
                printf("\033[?25h\033[H\033[J"); // Show cursor and clear console
                fflush(stdout);
                munmap(fb_ptr, finfo.smem_len);
                close(fb_fd);
                exit(0);
                return 0;
            } else if (strcmp(buffer, "sq\n") == 0) {
                save_and_exit();
                return 0;
            }
            int size = atoi(buffer);
            if (size > 0) {
                brush_size = size;
            } else {
                parse_color(buffer, brush_color);
            }
        }
        free(buffer);
        signed char mouse[3];
        ssize_t bytes = read(mousedev, mouse, sizeof(mouse));
        if (bytes == 3) {
            prev_x = x;
            prev_y = y;
            x += mouse[1];
            y -= mouse[2];
            if (x < (vinfo.xres - image_width) / 2)
                x = (vinfo.xres - image_width) / 2;
            if (y < (vinfo.yres - image_height) / 2)
                y = (vinfo.yres - image_height) / 2;
            if (x >= vinfo.xres - (vinfo.xres - image_width) / 2)
                x = vinfo.xres - (vinfo.xres - image_width) / 2 - 1;
            if (y >= vinfo.yres - (vinfo.yres - image_height) / 2)
                y = vinfo.yres - (vinfo.yres - image_height) / 2 - 1;
            memcpy(fb_ptr + (prev_y * finfo.line_length) + prev_x * (vinfo.bits_per_pixel / 8), pixel, 3);
            memcpy(pixel, fb_ptr + (y * finfo.line_length) + x * (vinfo.bits_per_pixel / 8), 3);
            char *ipixel = (char[]){~pixel[0], ~pixel[1], ~pixel[2]};
            memcpy(fb_ptr + (y * finfo.line_length) + x * (vinfo.bits_per_pixel / 8), ipixel, 3);
            if (mouse[0] & 1) {
                int count = 0;
                int **line_points = bresenham(prev_x, prev_y, x, y, &count);
                if (line_points) {
                    for (int i = 0; i < count; i++) {
                        draw_circle(line_points[i][0], line_points[i][1]);
                        free(line_points[i]);
                    }
                    free(line_points);
                }
            }
        }
        usleep(1000);
    }
}

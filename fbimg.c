#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <sys/mman.h>

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "Usage: %s <image_path>\n", argv[0]);
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
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
    uint32_t width, height;
    fread(&width, sizeof(uint32_t), 1, file);
    fread(&height, sizeof(uint32_t), 1, file);
    char color[4] = {0};
    fread(color, 1, 3, file);
    char *data = malloc(width * height * 3);
    fread(data, 1, width * height * 3, file);
    fclose(file);

    // Open the framebuffer device
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        return 1;
    }

    // Get variable screen info
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error getting screen info");
        close(fb_fd);
        return 1;
    }

    // Get fixed screen info
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error getting fixed screen info");
        close(fb_fd);
        return 1;
    }

    // Map framebuffer memory
    size_t screensize = finfo.smem_len;
    char *fb_ptr = mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        perror("Error mapping framebuffer memory");
        close(fb_fd);
        return 1;
    }

    // Write pixels to framebuffer
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int offset = (i * vinfo.xres + j) * (vinfo.bits_per_pixel / 8);
            int data_offset = (i * width + j) * 3;
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
            uint32_t pixel = 0;
            if (vinfo.transp.length > 0) {
                *((char *)(fb_ptr + offset + (vinfo.transp.offset/8))) = alpha;
            }
            *((char *)(fb_ptr + offset + (vinfo.red.offset/8))) = red;
            *((char *)(fb_ptr + offset + (vinfo.green.offset/8))) = green;
            *((char *)(fb_ptr + offset + (vinfo.blue.offset/8))) = blue;
        }
    }

    // Unmap framebuffer memory
    munmap(fb_ptr, screensize);
    close(fb_fd);
    free(data);
}

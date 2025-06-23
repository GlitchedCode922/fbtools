#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <getopt.h>

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"help" , no_argument, NULL, 'h'},
        {"usage", no_argument, NULL, 'u'},
        {NULL, 0, NULL, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "hu", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [options] /dev/input/(keyboard_device_node)\n", argv[0]);
                printf("Options:\n");
                printf("  -h, --help     Show this help message\n");
                printf("  -u, --usage    Show usage information\n");
                return 0;
            case 'u':
                printf("This program captures screenshots when the Print Screen or F5 key is pressed.\n");
                return 0;
            default:
                fprintf(stderr, "Usage: %s /dev/input/(keyboard_device_node)\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "Usage: %s /dev/input/(keyboard_device_node)\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }

    // Child process becomes session leader
    if (setsid() < 0) {
        perror("setsid failed");
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure the daemon cannot acquire a terminal
    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        // First child exits
        exit(EXIT_SUCCESS);
    }

    // Change working directory to root
    if (chdir("/") < 0) {
        perror("chdir failed");
        exit(EXIT_FAILURE);
    }

    // Redirect standard file descriptors to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        perror("open /dev/null failed");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    const char *device = argv[optind];
    fd = open(device, O_RDONLY);
    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) { // Key press event
            if (ev.code == KEY_PRINT || ev.code == KEY_F5) {
                int fb_fd = open("/dev/fb0", O_RDONLY);
                struct fb_var_screeninfo vinfo;
                if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
                    close(fb_fd);
                    return 1;
                }
                struct fb_fix_screeninfo finfo;
                if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
                    close(fb_fd);
                    return 1;
                }
                char *header = (char *)malloc(16);
                strncpy(header, "FBIMG", 5);
                memcpy(header + 5, &vinfo.xres, sizeof(uint32_t));
                memcpy(header + 9, &vinfo.yres, sizeof(uint32_t));
                strncpy(header + 13, "RGB", 3);
                char *image = (char *)malloc(vinfo.xres * vinfo.yres * 3);
                char *fb_ptr = (char *)mmap(NULL, finfo.smem_len, PROT_READ, MAP_SHARED, fb_fd, 0);
                if (fb_ptr == MAP_FAILED) {
                    close(fb_fd);
                    free(header);
                    free(image);
                    return 1;
                }
                for (int i=0;i<vinfo.yres;i++) {
                    for (int v=0;v<vinfo.xres;v++) {
                        int px = (i * finfo.line_length) + v * (vinfo.bits_per_pixel / 8);
                        unsigned char r, g, b;
                        r = fb_ptr[px + (vinfo.red.offset / 8)];
                        g = fb_ptr[px + (vinfo.green.offset / 8)];
                        b = fb_ptr[px + (vinfo.blue.offset / 8)];
                        image[(i * vinfo.xres + v) * 3] = r;
                        image[(i * vinfo.xres + v) * 3 + 1] = g;
                        image[(i * vinfo.xres + v) * 3 + 2] = b;
                    }
                }
                char output_file[256];
                snprintf(output_file, sizeof(output_file), "/tmp/screenshot_%ld.fbimg", time(NULL));
                FILE *output = fopen(output_file, "wb");
                if (!output) {
                    munmap(fb_ptr, finfo.smem_len);
                    free(header);
                    free(image);
                    close(fb_fd);
                    return 1;
                }
                fwrite(header, 1, 16, output);
                fwrite(image, 1, vinfo.xres * vinfo.yres * 3, output);
                fclose(output);
                close(fb_fd);
                munmap(fb_ptr, finfo.smem_len);
                free(header);
                free(image);
            }
        }
    }

    close(fd);
    return 0;
}

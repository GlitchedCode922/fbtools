#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "thirdparty/lodepng/lodepng.h"

int main(int argc, char *argv[]) {
    const struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [options] input output\n", argv[0]);
                printf("Options:\n");
                printf("  -h, --help       Show this help message\n");
                printf("  -v, --version    Show version information\n");
                return 0;
            case 'v':
                printf("FBTools %s v1.0\n", argv[0]);
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                return 1;
        }
    }

    char *input_file = NULL;
    char *output_file = NULL;

    // Parse remaining arguments
    if (optind + 1 < argc) {
        input_file = argv[optind];
        output_file = argv[optind + 1];
    } else {
        fprintf(stderr, "No input or output files specified.\n");
        return 1;
    }

    unsigned char *image;
    unsigned width, height;
    unsigned error = lodepng_decode32_file(&image, &width, &height, input_file);
    if (error) {
        fprintf(stderr, "Error decoding PNG: %s\n", lodepng_error_text(error));
        return 1;
    }
    FILE *output = fopen(output_file, "wb");
    if (!output) {
        fprintf(stderr, "Error opening output file: %s\n", output_file);
        free(image);
        return 1;
    }

    char header[16];
    const char *magic = "FBIMG";
    const char *rgb = "RGB";
    uint32_t width_uint32 = (uint32_t)width;
    uint32_t height_uint32 = (uint32_t)height;
    char *converted_img = malloc(width * height * 3);

    memcpy(header, magic, 5);
    memcpy(header + 5, &width_uint32, sizeof(uint32_t));
    memcpy(header + 9, &height_uint32, sizeof(uint32_t));
    memcpy(header + 13, rgb, 3);

    // Write header to file
    fwrite(header, sizeof(header), 1, output);
    // Write image data to file

    for (int px=0; px<width*height; px++) {
        int r = image[px * 4];
        int g = image[px * 4 + 1];
        int b = image[px * 4 + 2];
        converted_img[px * 3] = r;
        converted_img[px * 3 + 1] = g;
        converted_img[px * 3 + 2] = b;
    }
    fwrite(converted_img, 1, width * height * 3, output);

    fclose(output);
    free(converted_img);
    free(image);

    return 0;
}

#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
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

    if (optind + 1 < argc) {
        input_file = argv[optind];
        output_file = argv[optind + 1];
    } else {
        fprintf(stderr, "No input or output files specified.\n");
        return 1;
    }
    FILE *input = fopen(input_file, "rb");
    if (!input) {
        fprintf(stderr, "Error opening input file: %s\n", input_file);
        return 1;
    }
    char magic[6] = {0};
    fread(magic, 1, 5, input);
    if (strcmp(magic, "FBIMG") != 0 ) {
        fprintf(stderr, "Invalid file format: %s\n", input_file);
        fclose(input);
        return 1;
    }

    uint32_t width, height;
    fread(&width, sizeof(uint32_t), 1, input);
    fread(&height, sizeof(uint32_t), 1, input);

    char channels[4] = {0};
    fread(&channels, 1, 3, input);

    char *data = malloc(width * height * 3);
    fread(data, 3, width * height, input);
    fclose(input);

    char *image = malloc(width * height * 4);
    for (uint32_t i = 0; i < width * height; i++) {
        if (strcmp(channels, "BGR") != 0) {
            image[i * 4 + 0] = data[i * 3 + 0]; // R
            image[i * 4 + 1] = data[i * 3 + 1]; // G
            image[i * 4 + 2] = data[i * 3 + 2]; // B
        } else {
            image[i * 4 + 0] = data[i * 3 + 2]; // B
            image[i * 4 + 1] = data[i * 3 + 1]; // G
            image[i * 4 + 2] = data[i * 3 + 0]; // R
        }
        image[i * 4 + 3] = 255; // A
    }
    free(data);

    unsigned error = lodepng_encode32_file(output_file, (unsigned char *)image, width, height);
    if (error) {
        fprintf(stderr, "Error encoding PNG: %s\n", lodepng_error_text(error));
        free(image);
        return 1;
    }
    free(image);

    return 0;
}

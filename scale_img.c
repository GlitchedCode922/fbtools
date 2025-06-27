#include "include/scale_img.h"

#include <stdbool.h>
#include <stdlib.h>

int floorpx(float x) {
    int i = (int)x;
    return (x < i) ? i - 1 : i;
}

int calc_index(int width, int height, int x, int y) {
    return (y * width + x) * 3;
}

unsigned char lerp(unsigned char a, unsigned char b, float t) {
    float result = a + t * (b - a);
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return (unsigned char)(result);
}

char *scale_image(char *image, bool bgr, int width, int height, int new_width, int new_height) {
    char *scaled_image = (char *)malloc(new_width * new_height * 3);
    char *converted_image = (char *)malloc(width * height * 3);
    if (bgr) {
        // Convert BGR to RGB
        for (int i = 0; i < width * height * 3; i += 3) {
            converted_image[i] = image[i + 2]; // R
            converted_image[i + 1] = image[i + 1]; // G
            converted_image[i + 2] = image[i]; // B
        }
    } else {
        // Copy RGB directly
        for (int i = 0; i < width * height * 3; i++) {
            converted_image[i] = image[i];
        }
    }

    for (int w = 0; w < new_width; w++) {
        for (int h = 0; h < new_height; h++) {
            float orig_x = w * ((float)width / new_width);
            float orig_y = h * ((float)height / new_height);
            int x0 = floorpx(orig_x);
            int y0 = floorpx(orig_y);
            int x1 = (x0 + 1 < width) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < height) ? y0 + 1 : y0;
            char p1r = lerp(converted_image[calc_index(width, height, x0, y0)], converted_image[calc_index(width, height, x1, y0)], orig_x - x0);
            char p1g = lerp(converted_image[calc_index(width, height, x0, y0) + 1], converted_image[calc_index(width, height, x1, y0) + 1], orig_x - x0);
            char p1b = lerp(converted_image[calc_index(width, height, x0, y0) + 2], converted_image[calc_index(width, height, x1, y0) + 2], orig_x - x0);
            char p2r = lerp(converted_image[calc_index(width, height, x0, y1)], converted_image[calc_index(width, height, x1, y1)], orig_x - x0);
            char p2g = lerp(converted_image[calc_index(width, height, x0, y1) + 1], converted_image[calc_index(width, height, x1, y1) + 1], orig_x - x0);
            char p2b = lerp(converted_image[calc_index(width, height, x0, y1) + 2], converted_image[calc_index(width, height, x1, y1) + 2], orig_x - x0);
            scaled_image[calc_index(new_width, new_height, w, h)] = lerp(p1r, p2r, orig_y - y0);
            scaled_image[calc_index(new_width, new_height, w, h) + 1] = lerp(p1g, p2g, orig_y - y0);
            scaled_image[calc_index(new_width, new_height, w, h) + 2] = lerp(p1b, p2b, orig_y - y0);
        }
    }

    return scaled_image;
}

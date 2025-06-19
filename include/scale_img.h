#pragma once
#include <stdbool.h>

int floorpx(float x);
int calc_index(int width, int height, int x, int y);
unsigned char lerp(unsigned char a, unsigned char b, float t);
char *scale_image(char *image, bool bgr, int width, int height, int new_width, int new_height);

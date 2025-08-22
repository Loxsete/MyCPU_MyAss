#ifndef WINDOW_H
#define WINDOW_H
#include <raylib.h>
#include "cpu.h"
#include "bios.h"

typedef struct {
    int width;
    int height;
    int output_flash;
    Font font; // Add font to Window struct
} Window;

Window* window_init();
void window_cleanup(Window* win);
void window_render(Window* win, BIOS* bios, CPU* cpu);

#endif

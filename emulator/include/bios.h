#ifndef BIOS_H
#define BIOS_H
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include "cpu.h"

// Settings
#define MAX_FILES 100
#define INPUT_BUFFER_SIZE 256

typedef struct {
    // File selection
    char* file_list[MAX_FILES];
    int file_count;
    int selected_file;
    char* program_file;
    // Program output (via INT 2)
    char* program_output;
    // Screen state
    int initial_screen;
    // Input buffer for READ_LINE
    size_t input_length;
    char input_buffer[INPUT_BUFFER_SIZE];
    // Monitor mode (for debugging)
    int monitor_mode;
    // Flag to enable input polling for READ_LINE
    int read_line_active;
} BIOS;

// Initialization and cleanup
BIOS* bios_init();
void bios_cleanup(BIOS* bios);
// Interrupt handling (including keyboard)
void bios_handle_interrupt(CPU* cpu, BIOS* bios);
// Poll physical devices (Raylib wrapper) - call every frame when READ_LINE is active
void bios_poll_input(BIOS* bios);

#endif

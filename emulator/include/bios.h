#ifndef BIOS_H
#define BIOS_H
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include "cpu.h"
#include "disk.h"
#define MAX_FILES 100
#define INPUT_BUFFER_SIZE 256
typedef struct {
    char* file_list[MAX_FILES];
    int file_count;
    int selected_file;
    char* program_file;
    char* program_output;
    int initial_screen;
    size_t input_length;
    char input_buffer[INPUT_BUFFER_SIZE];
    int monitor_mode;
    int read_line_active;
    Disk* disk;
} BIOS;
BIOS* bios_init();
void bios_cleanup(BIOS* bios);
void bios_handle_interrupt(CPU* cpu, BIOS* bios);
void bios_poll_input(BIOS* bios);
#endif

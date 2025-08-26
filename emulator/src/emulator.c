#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include "cpu.h"
#include "bios.h"
#include "window.h"
typedef struct {
    CPU* cpu;
    BIOS* bios;
    Window* window;
} Emulator;
static Emulator* emulator_init(size_t memory_size, size_t stack_size) {
    Emulator* emu = (Emulator*)malloc(sizeof(Emulator));
    if (!emu) { printf("Error: Failed to allocate memory for emulator!\n"); exit(1); }
    emu->cpu = cpu_init(memory_size, stack_size);
    emu->bios = bios_init();
    emu->window = window_init();
    emu->bios->initial_screen = 1;
    return emu;
}
static void emulator_cleanup(Emulator* emu) {
    cpu_cleanup(emu->cpu);
    bios_cleanup(emu->bios);
    window_cleanup(emu->window);
    free(emu);
}
static void handle_menu_input(BIOS* bios, CPU* cpu) {
    if (bios->initial_screen) {
        if (IsKeyPressed(KEY_UP)) { if (bios->selected_file > 0) bios->selected_file--; }
        if (IsKeyPressed(KEY_DOWN)) { if (bios->selected_file < bios->file_count - 1) bios->selected_file++; }
        if (IsKeyPressed(KEY_ENTER) && bios->file_count > 0) {
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "bin/%s", bios->file_list[bios->selected_file]);
            free(bios->program_file);
            bios->program_file = strdup(bios->file_list[bios->selected_file]);
            cpu_load_program(cpu, filepath);
            if (cpu->program_size > 0) {
                bios->initial_screen = 0;
                cpu->running = 1;
            } else {
                bios->program_file = NULL;
            }
        }
    } else {
        if (IsKeyPressed(KEY_Q)) {
            cpu->running = 0;
            bios->program_file = NULL;
            free(bios->program_output);
            bios->program_output = NULL;
            bios->initial_screen = 1;
            bios->read_line_active = 0;
        }
    }
}
static void emulator_run(Emulator* emu) {
    while (!WindowShouldClose()) {
        bios_poll_input(emu->bios);
        handle_menu_input(emu->bios, emu->cpu);
        if (emu->bios->program_file != NULL && emu->cpu->running && !emu->bios->initial_screen) {
            cpu_execute_instruction(emu->cpu);
            bios_handle_interrupt(emu->cpu, emu->bios);
        }
        window_render(emu->window, emu->bios, emu->cpu);
    }
}
int main(int argc, char* argv[]) {
    size_t memory_size = 4096;
    size_t stack_size = 1024;
    if (argc > 2) {
        memory_size = (size_t)atoi(argv[1]);
        stack_size = (size_t)atoi(argv[2]);
    }
    Emulator* emu = emulator_init(memory_size, stack_size);
    emulator_run(emu);
    emulator_cleanup(emu);
    return 0;
}

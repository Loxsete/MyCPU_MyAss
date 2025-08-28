#include "bios.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <raylib.h>

#define HISTORY_SIZE 50
#define MAX_FILENAME 64

BIOS* bios_init() {
    BIOS* bios = (BIOS*)calloc(1, sizeof(BIOS));
    if (!bios) {
        fprintf(stderr, "Error: Failed to allocate memory for BIOS!\n");
        exit(1);
    }

    bios->disk = disk_init();
    bios->history = (char**)calloc(HISTORY_SIZE, sizeof(char*));
    bios->history_count = 0;
    bios->history_index = -1;
    bios->initial_screen = 1;

    // Scan for .bin files
    DIR* dir = opendir("bin");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".bin") && bios->file_count < MAX_FILES) {
                bios->file_list[bios->file_count] = strndup(entry->d_name, MAX_FILENAME);
                bios->file_count++;
            }
        }
        closedir(dir);
    } else {
        fprintf(stderr, "BIOS: Failed to open bin directory: %s\n", strerror(errno));
    }

    return bios;
}

void bios_cleanup(BIOS* bios) {
    for (int i = 0; i < bios->file_count; i++) {
        free(bios->file_list[i]);
    }
    for (int i = 0; i < bios->history_count; i++) {
        free(bios->history[i]);
    }
    free(bios->history);
    free(bios->program_output);
    free(bios->program_file);
    disk_cleanup(bios->disk);
    free(bios);
}

void bios_poll_input(BIOS* bios) {
    if (!bios->read_line_active) return;

    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch <= 126 && bios->input_length < INPUT_BUFFER_SIZE - 1) {
            bios->input_buffer[bios->input_length++] = (char)ch;
            bios->input_buffer[bios->input_length] = '\0';
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && bios->input_length > 0) {
        bios->input_length--;
        bios->input_buffer[bios->input_length] = '\0';
    }

    if (IsKeyPressed(KEY_UP) && bios->history_index < bios->history_count - 1) {
        bios->history_index++;
        strncpy(bios->input_buffer, bios->history[bios->history_index], INPUT_BUFFER_SIZE);
        bios->input_length = strlen(bios->input_buffer);
    }

    if (IsKeyPressed(KEY_DOWN) && bios->history_index >= 0) {
        bios->history_index--;
        if (bios->history_index == -1) {
            bios->input_buffer[0] = '\0';
            bios->input_length = 0;
        } else {
            strncpy(bios->input_buffer, bios->history[bios->history_index], INPUT_BUFFER_SIZE);
            bios->input_length = strlen(bios->input_buffer);
        }
    }

    if (IsKeyPressed(KEY_ENTER) && bios->input_length > 0) {
        if (bios->history_count < HISTORY_SIZE) {
            bios->history[bios->history_count] = strdup(bios->input_buffer);
            bios->history_count++;
        } else {
            free(bios->history[0]);
            memmove(bios->history, bios->history + 1, (HISTORY_SIZE - 1) * sizeof(char*));
            bios->history[HISTORY_SIZE - 1] = strdup(bios->input_buffer);
        }
        bios->history_index = -1;
    }
}

static void copy_line_to_mem_and_clear(CPU* cpu, BIOS* bios, uint16_t addr) {
    uint8_t* mem = (uint8_t*)cpu->memory;
    size_t max = cpu->memory_size * sizeof(uint16_t);
    size_t n = bios->input_length;
    if (addr + n + 1 > max) {
        n = (max > addr) ? (max - addr - 1) : 0;
    }
    if (n > 0) {
        memcpy(&mem[addr], bios->input_buffer, n);
    }
    mem[addr + n] = 0;
    bios->input_length = 0;
    bios->input_buffer[0] = '\0';
    bios->read_line_active = 0;
}

void bios_handle_interrupt(CPU* cpu, BIOS* bios) {
    if (cpu->interrupt == 0) return;

    switch (cpu->interrupt) {
        case 2: { // Print string
            uint16_t addr = cpu->registers[0];
            uint8_t* memory_bytes = (uint8_t*)cpu->memory;
            size_t max_addr = cpu->memory_size * sizeof(uint16_t);
            char buffer[1024];
            size_t i = 0;
            while (addr < max_addr && i < sizeof(buffer) - 1) {
                uint8_t byte = memory_bytes[addr++];
                if (byte == 0) break;
                buffer[i++] = (char)byte;
            }
            buffer[i] = '\0';
            free(bios->program_output);
            bios->program_output = strdup(buffer);
            printf("Output: %s\n", bios->program_output);
            break;
        }
        case 3: { // Output control
            uint8_t func = cpu->registers[0] & 0xFF;
            switch (func) {
                case 0x01: { // Append newline
                    size_t len = bios->program_output ? strlen(bios->program_output) : 0;
                    char* temp = malloc(len + 2);
                    if (temp) {
                        if (bios->program_output) strcpy(temp, bios->program_output);
                        temp[len] = '\n';
                        temp[len + 1] = '\0';
                        free(bios->program_output);
                        bios->program_output = temp;
                    }
                    break;
                }
                case 0x02: { // Clear output
                    free(bios->program_output);
                    bios->program_output = strdup("");
                    break;
                }
                default:
                    cpu->zero_flag = 1;
                    break;
            }
            break;
        }
        case 4: { // Delay
            uint16_t delay_ms = cpu->registers[0];
            WaitTime((float)delay_ms / 1000.0f);
            break;
        }
        case 6: { // Load program
            uint16_t file_idx = cpu->registers[0];
            if (file_idx < bios->file_count) {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "bin/%s", bios->file_list[file_idx]);
                free(bios->program_file);
                bios->program_file = strdup(bios->file_list[file_idx]);
                cpu_load_program(cpu, filepath);
                cpu->running = (cpu->program_size > 0);
                cpu->zero_flag = cpu->running ? 0 : 1;
            } else {
                cpu->zero_flag = 1;
            }
            break;
        }
        case 1: { // Keyboard input
            uint8_t func = cpu->registers[0] & 0xFF;
            switch (func) {
                case 0x01: { // Single key (press)
                    int ch = GetCharPressed();
                    if (ch > 0) {
                        cpu->registers[0] = (uint16_t)ch;
                        cpu->zero_flag = 0;
                    } else if (IsKeyPressed(KEY_BACKSPACE)) {
                        cpu->registers[0] = 8;
                        cpu->zero_flag = 0;
                    } else if (IsKeyPressed(KEY_ENTER)) {
                        cpu->registers[0] = '\n';
                        cpu->zero_flag = 0;
                    } else if (IsKeyPressed(KEY_ESCAPE)) {
                        cpu->registers[0] = 27;
                        cpu->zero_flag = 0;
                    }
                    break;
                }
                case 0x02: { // Single key (hold)
                    int ch = GetCharPressed();
                    if (ch > 0) {
                        cpu->registers[0] = (uint16_t)ch;
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_BACKSPACE)) {
                        cpu->registers[0] = 8;
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_ENTER)) {
                        cpu->registers[0] = '\n';
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_ESCAPE)) {
                        cpu->registers[0] = 27;
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_TAB)) {
                        cpu->registers[0] = 9;
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_UP)) {
                        cpu->registers[0] = 0xE000;
                        cpu->zero_flag = 0;
                    } else if (IsKeyDown(KEY_DOWN)) {
                        cpu->registers[0] = 0xE001;
                        cpu->zero_flag = 0;
                    } else {
                        cpu->registers[0] = 0;
                        cpu->zero_flag = 1;
                    }
                    break;
                }

                case 0x03: { // Read line
                    bios->read_line_active = 1;
                    uint16_t addr = cpu->registers[1];
                    copy_line_to_mem_and_clear(cpu, bios, addr);
                    cpu->zero_flag = 0;
                    break;
                }
                default:
                    cpu->zero_flag = 1;
                    break;
            }
            break;
        }
        case 9: { // Read line with history
            bios->read_line_active = 1;
            if (bios->input_length > 0) {
                copy_line_to_mem_and_clear(cpu, bios, 100);
                cpu->registers[0] = 100;
                cpu->zero_flag = 0;
            } else {
                cpu->registers[0] = 0;
                cpu->zero_flag = 1;
            }
            break;
        }
        case 10: { // Disk operations
            uint8_t func = cpu->registers[0] & 0xFF;
            switch (func) {
                case 0x01: { // Read
                    disk_read(bios->disk, cpu->registers[1], cpu->registers[2], (uint8_t*)cpu->memory);
                    cpu->zero_flag = (disk_status(bios->disk) == 0) ? 0 : 1;
                    break;
                }
                case 0x02: { // Write
                    disk_write(bios->disk, cpu->registers[1], cpu->registers[2], (uint8_t*)cpu->memory);
                    cpu->zero_flag = (disk_status(bios->disk) == 0) ? 0 : 1;
                    break;
                }
                case 0x03: { // Status
                    cpu->registers[0] = disk_status(bios->disk);
                    cpu->zero_flag = (cpu->registers[0] == 0) ? 0 : 1;
                    break;
                }
                case 0x04: { // Create file
                    char filename[MAX_FILENAME];
                    strncpy(filename, (char*)cpu->memory + cpu->registers[1], MAX_FILENAME - 1);
                    filename[MAX_FILENAME - 1] = '\0';
                    disk_create_file(bios->disk, filename);
                    cpu->zero_flag = (disk_status(bios->disk) == 0) ? 0 : 1;
                    break;
                }
                case 0x05: { // Delete file
                    char filename[MAX_FILENAME];
                    strncpy(filename, (char*)cpu->memory + cpu->registers[1], MAX_FILENAME - 1);
                    filename[MAX_FILENAME - 1] = '\0';
                    disk_delete_file(bios->disk, filename);
                    cpu->zero_flag = (disk_status(bios->disk) == 0) ? 0 : 1;
                    break;
                }
                default:
                    cpu->zero_flag = 1;
                    break;
            }
            break;
        }
    }
    cpu->interrupt = 0;
}

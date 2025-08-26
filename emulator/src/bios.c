#include "bios.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <raylib.h>
BIOS* bios_init() {
    BIOS* bios = (BIOS*)malloc(sizeof(BIOS));
    if (!bios) {
        printf("Error: Failed to allocate memory for BIOS!\n");
        exit(1);
    }
    memset(bios, 0, sizeof(*bios));
    DIR* dir = opendir("bin");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".bin") != NULL) {
                bios->file_list[bios->file_count] = strdup(entry->d_name);
                bios->file_count++;
                if (bios->file_count >= MAX_FILES) {
                    printf("BIOS: Too many .bin files, limiting to %d\n", MAX_FILES);
                    break;
                }
            }
        }
        closedir(dir);
    } else {
        printf("BIOS: Failed to open bin directory!\n");
    }
    bios->initial_screen = 1;
    bios->disk = disk_init();
    return bios;
}
void bios_cleanup(BIOS* bios) {
    for (int i = 0; i < bios->file_count; i++) {
        free(bios->file_list[i]);
    }
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
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (bios->input_length > 0) {
            bios->input_length--;
            bios->input_buffer[bios->input_length] = '\0';
        }
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
        case 2: {
            uint16_t addr = cpu->registers[0];
            uint8_t* memory_bytes = (uint8_t*)cpu->memory;
            size_t max_addr = cpu->memory_size * sizeof(uint16_t);
            size_t buffer_size = 1024;
            char* buffer = (char*)malloc(buffer_size);
            if (!buffer) {
                printf("Error: Failed to allocate buffer for output!\n");
                break;
            }
            size_t i = 0;
            while (addr < max_addr && i < buffer_size - 1) {
                uint8_t byte = memory_bytes[addr++];
                if (byte == 0) break;
                buffer[i++] = (char)byte;
            }
            buffer[i] = '\0';
            free(bios->program_output);
            bios->program_output = strdup(buffer);
            printf("Output: %s\n", bios->program_output);
            free(buffer);
            break;
        }
        case 3: {
            uint8_t func = cpu->registers[0] & 0xFF;
            switch (func) {
                case 0x01: {
                    if (bios->program_output) {
                        size_t len = strlen(bios->program_output);
                        char* temp = malloc(len + 2);
                        if (temp) {
                            strcpy(temp, bios->program_output);
                            temp[len] = '\n';
                            temp[len + 1] = '\0';
                            free(bios->program_output);
                            bios->program_output = temp;
                        }
                    } else {
                        bios->program_output = strdup("\n");
                    }
                    break;
                }
                case 0x02: {
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
        case 4: {
            uint16_t delay_ms = cpu->registers[0];
            WaitTime((float)delay_ms / 1000.0f);
            break;
        }
        case 6: {
            if (cpu->registers[0] < bios->file_count) {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "bin/%s", bios->file_list[cpu->registers[0]]);
                free(bios->program_file);
                bios->program_file = strdup(bios->file_list[cpu->registers[0]]);
                cpu_load_program(cpu, filepath);
                if (cpu->program_size > 0) {
                    cpu->running = 1;
                } else {
                    bios->program_file = NULL;
                }
            }
            break;
        }
        case 1: {
            uint8_t func = (uint8_t)(cpu->registers[0] & 0xFF);
            switch (func) {
                case 0x01: {
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
                    } else if (IsKeyPressed(KEY_TAB)) {
                        cpu->registers[0] = 9;
                        cpu->zero_flag = 0;
                    } else {
                        cpu->registers[0] = 0;
                        cpu->zero_flag = 1;
                    }
                    break;
                }
                case 0x02: {
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
                    } else {
                        cpu->registers[0] = 0;
                        cpu->zero_flag = 1;
                    }
                    break;
                }
                case 0x03: {
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
        case 9: {
            bios->read_line_active = 1;
            if (bios->input_length > 0) {
                copy_line_to_mem_and_clear(cpu, bios, 100);
                cpu->registers[0] = 100;
            } else {
                cpu->registers[0] = 0;
            }
            break;
        }
        case 10: {
            uint8_t func = cpu->registers[0] & 0xFF;
            switch (func) {
                case 0x01: {
                    disk_read(bios->disk, cpu->registers[1], cpu->registers[2], (uint8_t*)cpu->memory);
                    cpu->zero_flag = (bios->disk->last_error == 0) ? 0 : 1;
                    break;
                }
                case 0x02: {
                    disk_write(bios->disk, cpu->registers[1], cpu->registers[2], (uint8_t*)cpu->memory);
                    cpu->zero_flag = (bios->disk->last_error == 0) ? 0 : 1;
                    break;
                }
                case 0x03: {
                    uint16_t status = disk_status(bios->disk);
                    cpu->registers[0] = status;
                    cpu->zero_flag = (status == 0) ? 0 : 1;
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

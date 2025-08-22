#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

Window* window_init() {
    Window* win = (Window*)malloc(sizeof(Window));
    if (!win) {
        printf("Error: Failed to allocate memory for Window!\n");
        exit(1);
    }
    win->width = 800;
    win->height = 600;
    win->output_flash = 0;
    SetTraceLogLevel(LOG_NONE);
    InitWindow(win->width, win->height, "CPU Emulator");
    SetTargetFPS(60);
    win->font = LoadFont("font/font.ttf"); // Load custom font
    if (win->font.texture.id == 0) { // Check if font loaded successfully
        printf("Error: Failed to load font from font/font.ttf!\n");
        exit(1);
    }
    return win;
}

void window_cleanup(Window* win) {
    UnloadFont(win->font); // Unload custom font
    CloseWindow();
    free(win);
}

void window_render(Window* win, BIOS* bios, CPU* cpu) {
    BeginDrawing();
    ClearBackground((Color){0, 0, 0, 255}); // Pure black background

    // Define a consistent color palette
    Color text = {255, 255, 255, 255}; // Pure white for text
    Color glow = {255, 255, 255, 80}; // Subtle white glow for highlights
    Color highlight = {50, 50, 80, 255}; // Darker highlight for selection
    Color accent = {255, 255, 255, 255}; // Pure white for headers
    Color border = {60, 60, 80, 255}; // Subtle border color

    if (bios->initial_screen) {
        // Title
        const char* title = "Select Binary File";
        float title_x = win->width / 2 - MeasureTextEx(win->font, title, 28, 1).x / 2;
        DrawTextEx(win->font, title, (Vector2){title_x + 2, 82}, 28, 1, glow); // Shadow effect
        DrawTextEx(win->font, title, (Vector2){title_x, 80}, 28, 1, accent); // Main title

        if (bios->file_count == 0) {
            const char* no_files = "No .bin files detected!";
            float no_files_x = win->width / 2 - MeasureTextEx(win->font, no_files, 20, 1).x / 2;
            DrawTextEx(win->font, no_files, (Vector2){no_files_x, 200}, 20, 1, text);
        } else {
            // File selection list with improved spacing and hover effect
            for (int i = 0; i < bios->file_count; i++) {
                float y_pos = 160 + i * 45;
                if (i == bios->selected_file) {
                    DrawRectangleRounded((Rectangle){win->width / 2 - 210, y_pos - 5, 420, 35}, 0.2, 8, highlight);
                    DrawRectangleLinesEx((Rectangle){win->width / 2 - 210, y_pos - 5, 420, 35}, 1, border);
                }
                float file_x = win->width / 2 - MeasureTextEx(win->font, bios->file_list[i], 20, 1).x / 2;
                DrawTextEx(win->font, bios->file_list[i], (Vector2){file_x, y_pos}, 20, 1, text);
            }
            // Instructions with better alignment
            const char* instructions = "UP/DOWN: Navigate | ENTER: Load";
            float instr_x = win->width / 2 - MeasureTextEx(win->font, instructions, 16, 1).x / 2;
            DrawTextEx(win->font, instructions, (Vector2){instr_x, win->height - 50}, 16, 1, text);
        }
    } else {
        // Running program UI
        char buf[256];
        snprintf(buf, sizeof(buf), "Running: %s", bios->program_file ? bios->program_file : "Unknown");
        DrawTextEx(win->font, buf, (Vector2){20, 20}, 18, 1, accent); // Program name in white

        // Program output with multi-line support
        if (bios->program_output) {
            DrawTextEx(win->font, "Output:", (Vector2){20, 50}, 22, 1, glow); // Shadow
            DrawTextEx(win->font, "Output:", (Vector2){20, 50}, 22, 1, accent); // Header

            Color output_color = (win->output_flash > 0) ? (Color){255, 255, 255, 255} : text; // White flash effect
            float y_pos = 80;
            char* output = bios->program_output;
            char* line_start = output;
            char* line_end;

            // Разбиваем текст на строки по '\n'
            while ((line_end = strchr(line_start, '\n')) != NULL) {
                size_t line_len = line_end - line_start;
                char line[256];
                strncpy(line, line_start, line_len);
                line[line_len] = '\0'; // Завершаем строку
                DrawTextEx(win->font, line, (Vector2){20, y_pos}, 24, 1, output_color);
                y_pos += 30; // Интервал между строками
                line_start = line_end + 1;
            }
            // Отрисовываем последнюю строку, если она есть
            if (*line_start) {
                DrawTextEx(win->font, line_start, (Vector2){20, y_pos}, 24, 1, output_color);
            }
            if (win->output_flash > 0) win->output_flash--;
        }

        // Current Instruction
        if (cpu->pc < cpu->program_size) {
            uint16_t instr = cpu->memory[cpu->pc];
            uint8_t op = (instr >> 11) & 0x1F;
            uint8_t r1 = (instr >> 8) & 0x7;
            uint16_t val = instr & 0xFF;
            const char* os = "UNKNOWN";
            switch (op) {
                case 0: os = "NOP"; break; // Исправляем опкоды
                case 1: os = "HLT"; break;
                case 2: os = "MOV"; break;
                case 3: os = "ADD"; break;
                case 4: os = "SUB"; break;
                case 5: os = "MUL"; break;
                case 6: os = "DIV"; break;
                case 7: os = "MOD"; break;
                case 8: os = "AND"; break;
                case 9: os = "OR"; break;
                case 10: os = "XOR"; break;
                case 11: os = "NOT"; break;
                case 12: os = "NEG"; break;
                case 13: os = "SHL"; break;
                case 14: os = "SHR"; break;
                case 15: os = "CMP"; break;
                case 16: os = "PUSH"; break;
                case 17: os = "POP"; break;
                case 18: os = "PUSHA"; break;
                case 19: os = "POPA"; break;
                case 20: os = "INT"; break;
                case 21: os = "JMP"; break;
                case 22: os = "CALL"; break;
                case 23: os = "RET"; break;
                case 24: os = "JZ"; break;
                case 25: os = "JNZ"; break;
                case 26: os = "JG"; break;
                case 27: os = "JL"; break;
                case 28: os = "MOV_REG_MEM"; break; // mov reg, [mem]
                case 29: os = "MOV_MEM_REG"; break; // mov [mem], reg
            }
            char line[128];
            if (op == 0 || op == 1 || op == 18 || op == 19 || op == 23) {
                snprintf(line, sizeof(line), "PC: %u | %s", cpu->pc, os);
            } else if (op == 20 || op == 21 || op == 24 || op == 25 || op == 26 || op == 27) {
                snprintf(line, sizeof(line), "PC: %u | %s %u", cpu->pc, os, cpu->memory[cpu->pc]);
            } else if (op == 2 || op == 3 || op == 4 || op == 5 || op == 6 || op == 7 || op == 8 || op == 9 || op == 10 || op == 13 || op == 14 || op == 15 || op == 28 || op == 29) {
                snprintf(line, sizeof(line), "PC: %u | %s %s, %u", cpu->pc, os, 
                         (r1 == 0) ? "AX" : (r1 == 1) ? "BX" : (r1 == 2) ? "CX" : "DX", cpu->memory[cpu->pc]);
            } else {
                snprintf(line, sizeof(line), "PC: %u | %s %s", cpu->pc, os, 
                         (r1 == 0) ? "AX" : (r1 == 1) ? "BX" : (r1 == 2) ? "CX" : "DX");
            }
            DrawRectangleRounded((Rectangle){10, 450, 360, 40}, 0.2, 8, highlight);
            DrawRectangleLinesEx((Rectangle){10, 450, 360, 40}, 1, border);
            DrawTextEx(win->font, line, (Vector2){20, 460}, 16, 1, text);
        }
    }
    EndDrawing();
}

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
    win->width = GetScreenWidth();
    win->height = GetScreenHeight();
    
    SetTraceLogLevel(LOG_NONE);
    InitWindow(win->width, win->height, "Corx16 Emulator");
    SetTargetFPS(60);
    
    win->font = LoadFont("font/font.ttf");
    if (win->font.texture.id == 0) {
        printf("Warning: Failed to load font from font/font.ttf, trying fallback!\n");
        win->font = LoadFont("font/DejaVuSansMono.ttf");
        if (win->font.texture.id == 0) {
            printf("Error: Failed to load font from font/DejaVuSansMono.ttf!\n");
            exit(1);
        }
    }
    return win;
}

void window_cleanup(Window* win) {
    UnloadFont(win->font);
    CloseWindow();
    free(win);
}

void window_render(Window* win, BIOS* bios, CPU* cpu) {
    BeginDrawing();
    ClearBackground(BLACK);
    
    Color text = WHITE;
    float font_size = 20;
    float line_spacing = 26;

    if (bios->initial_screen) {
        const char* title = "Boot Menu";
        float title_x = win->width / 2 - MeasureTextEx(win->font, title, font_size, 1).x / 2;
        DrawTextEx(win->font, title, (Vector2){title_x, 40}, font_size, 1, text);

        if (bios->file_count == 0) {
            const char* no_files = "No bootable files found";
            float no_files_x = win->width / 2 - MeasureTextEx(win->font, no_files, font_size, 1).x / 2;
            DrawTextEx(win->font, no_files, (Vector2){no_files_x, win->height / 2}, font_size, 1, text);
        } else {
            for (int i = 0; i < bios->file_count; i++) {
                float y_pos = 100 + i * line_spacing;
                char file_text[256];
                snprintf(file_text, sizeof(file_text), "%s%s", 
                         (i == bios->selected_file) ? "> " : "  ", 
                         bios->file_list[i]);
                DrawTextEx(win->font, file_text, (Vector2){40, y_pos}, font_size, 1, text);
            }
            const char* instructions = "Up/Down: Navigate | Enter: Boot";
            float instr_x = win->width / 2 - MeasureTextEx(win->font, instructions, 14, 1).x / 2;
            DrawTextEx(win->font, instructions, (Vector2){instr_x, win->height - 40}, 14, 1, text);
        }
    } else {
        if (bios->program_output) {
            float y_pos = 20;
            char* output = bios->program_output;
            char* line_start = output;
            char* line_end;
            
            while ((line_end = strchr(line_start, '\n')) != NULL) {
                size_t line_len = line_end - line_start;
                char line[256];
                strncpy(line, line_start, line_len);
                line[line_len] = '\0';
                DrawTextEx(win->font, line, (Vector2){20, y_pos}, font_size, 1, text);
                y_pos += line_spacing;
                line_start = line_end + 1;
            }
            if (*line_start) {
                DrawTextEx(win->font, line_start, (Vector2){20, y_pos}, font_size, 1, text);
            }
        }

        if (bios->read_line_active) {
            char input[256];
            snprintf(input, sizeof(input), "> %s_", bios->input_buffer);
            DrawTextEx(win->font, input, (Vector2){20, win->height - line_spacing - 20}, font_size, 1, text);
        }
    }
    
    EndDrawing();
}

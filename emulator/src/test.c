#include <stdio.h>
#include <stdint.h>

#define ENC(op, reg, val) ((uint16_t)(((op) << 12) | ((reg) << 8) | ((val) & 0xFF)))

int main() {
    uint16_t prog[] = {
        ENC(0, 0, 100), // MOV R0, 100  ; Установить адрес для хранения ввода
        ENC(2, 0, 9),   // INT 9        ; Скопировать введенную строку в память по адресу R0
        ENC(0, 0, 100), // MOV R0, 100  ; Установить адрес строки для вывода
        ENC(2, 0, 2),   // INT 2        ; Вывести строку из памяти
        ENC(3, 0, 0),   // HLT          ; Остановить программу
    };

    FILE *f = fopen("keyboard.bin", "wb");
    if (!f) {
        printf("Error: Failed to open file for writing!\n");
        return 1;
    }
    fwrite(prog, sizeof(uint16_t), sizeof(prog) / sizeof(uint16_t), f);
    fclose(f);
    printf("Program written to keyboard.bin\n");
    return 0;
}

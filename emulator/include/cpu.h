#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stddef.h>

#define NUM_REGISTERS 4

typedef struct {
    uint16_t registers[NUM_REGISTERS]; // AX,BX,CX,DX
    uint16_t pc;
    uint16_t sp;
    size_t   memory_size;              // в словах (uint16_t)
    size_t   stack_size;               // в словах (uint16_t)
    uint16_t* memory;                  // слова
    size_t   program_size;             // в словах

    int      running;
    uint16_t interrupt;

    int      zero_flag;
    int      carry_flag;
    int      sign_flag;
} CPU;

CPU*    cpu_init(size_t memory_size, size_t stack_size);
void    cpu_load_program(CPU* cpu, const char* filename);
void    cpu_execute_instruction(CPU* cpu);
void    cpu_cleanup(CPU* cpu);

// байтовые доступы (для BIOS)
uint8_t cpu_read_byte (CPU* cpu, uint16_t address);
void    cpu_write_byte(CPU* cpu, uint16_t address, uint8_t value);

#endif

#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
CPU* cpu_init(size_t memory_size, size_t stack_size) {
    CPU* cpu = (CPU*)malloc(sizeof(CPU));
    if (!cpu) { printf("Error: Failed to allocate memory for CPU!\n"); exit(1); }
    memset(cpu, 0, sizeof(*cpu));
    cpu->sp = (uint16_t)(memory_size + stack_size);
    cpu->memory_size = memory_size;
    cpu->stack_size  = stack_size;
    cpu->memory = (uint16_t*)malloc((memory_size + stack_size) * sizeof(uint16_t));
    if (!cpu->memory) { printf("Error: Failed to allocate memory for CPU memory!\n"); exit(1); }
    memset(cpu->memory, 0, (memory_size + stack_size) * sizeof(uint16_t));
    cpu->running = 1;
    return cpu;
}
void cpu_cleanup(CPU* cpu) {
    free(cpu->memory);
    free(cpu);
}
void cpu_load_program(CPU* cpu, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Failed to open binary file %s! (errno: %s)\n", filename, strerror(errno));
        return;
    }
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t* cpu_bytes = (uint8_t*)cpu->memory;
    size_t read = fread(cpu_bytes, 1, file_size, file);
    fclose(file);
    if (read != file_size) {
        printf("Error: Read %zu bytes, expected %zu from %s!\n", read, file_size, filename);
        return;
    }
    uint16_t org_address = 0x1000;
    cpu->pc = org_address / sizeof(uint16_t);
    size_t total_words = cpu->memory_size;
    cpu->program_size = total_words;
    printf("Program loaded: file_size=%zu bytes, PC=0x%04x (%u words), program_size=%zu words\n",
           file_size, org_address, cpu->pc, cpu->program_size);
}
uint8_t cpu_read_byte(CPU* cpu, uint16_t address) {
    size_t max = cpu->memory_size * sizeof(uint16_t);
    if (address >= max) return 0;
    return ((uint8_t*)cpu->memory)[address];
}
void cpu_write_byte(CPU* cpu, uint16_t address, uint8_t value) {
    size_t max = cpu->memory_size * sizeof(uint16_t);
    if (address >= max) return;
    ((uint8_t*)cpu->memory)[address] = value;
}
void cpu_execute_instruction(CPU* cpu) {
    if (!cpu->running) {
        return;
    }
    if (cpu->pc >= cpu->program_size) {
        printf("Error: PC (%u) out of program bounds (%zu)!\n", cpu->pc, cpu->program_size);
        cpu->running = 0;
        return;
    }
    uint16_t instruction = cpu->memory[cpu->pc];
    if (instruction == 0) {
        printf("Warning: Encountered zero instruction at PC %u, halting\n", cpu->pc);
        cpu->running = 0;
        return;
    }
    uint8_t opcode = (instruction >> 11) & 0x1F;
    uint8_t reg1 = (instruction >> 8) & 0x7;
    uint8_t mode = instruction & 0x1F;
    uint16_t value = 0;
    uint8_t reg2 = 0;
    int is_reg2 = (mode == 2);
    if (mode == 3 || mode == 4 || mode == 5 || mode == 6 || mode == 7) {
        if (cpu->pc + 1 >= cpu->program_size) {
            printf("Error: PC (%u) out of bounds for immediate value!\n", cpu->pc);
            cpu->running = 0;
            return;
        }
        value = cpu->memory[cpu->pc + 1];
        cpu->pc++;
    } else if (is_reg2) {
        reg2 = (instruction >> 5) & 0x7;
    }
    printf("Executing PC: %u, Instruction: 0x%04x (opcode: %u, reg1: %u, mode: %u, value: 0x%04x, is_reg2: %d, reg2: %u)\n",
           cpu->pc, instruction, opcode, reg1, mode, value, is_reg2, reg2);
    cpu->pc++;
    if (reg1 >= NUM_REGISTERS || (is_reg2 && reg2 >= NUM_REGISTERS)) {
        printf("Error: Invalid register index (reg1: %u, reg2: %u) at PC %u!\n", reg1, reg2, cpu->pc - 1);
        cpu->running = 0;
        return;
    }
    switch (opcode) {
        case 0:
            break;
        case 1:
            cpu->running = 0;
            break;
        case 2:
            if (is_reg2) {
                cpu->registers[reg1] = cpu->registers[reg2];
            } else if (mode == 3 || mode == 4) {
                cpu->registers[reg1] = value;
            } else {
                printf("Error: Invalid MOV mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 3:
            if (is_reg2) {
                uint32_t result = cpu->registers[reg1] + cpu->registers[reg2];
                cpu->carry_flag = (result > 0xFFFF) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else if (mode == 3) {
                uint32_t result = cpu->registers[reg1] + value;
                cpu->carry_flag = (result > 0xFFFF) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else {
                printf("Error: Invalid ADD mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 4:
            if (is_reg2) {
                uint32_t result = cpu->registers[reg1] - cpu->registers[reg2];
                cpu->carry_flag = (cpu->registers[reg1] < cpu->registers[reg2]) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else if (mode == 3) {
                uint32_t result = cpu->registers[reg1] - value;
                cpu->carry_flag = (cpu->registers[reg1] < value) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else {
                printf("Error: Invalid SUB mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 5:
            if (is_reg2) {
                uint32_t result = cpu->registers[reg1] * cpu->registers[reg2];
                cpu->carry_flag = (result > 0xFFFF) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else if (mode == 3) {
                uint32_t result = cpu->registers[reg1] * value;
                cpu->carry_flag = (result > 0xFFFF) ? 1 : 0;
                cpu->registers[reg1] = result & 0xFFFF;
            } else {
                printf("Error: Invalid MUL mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 6:
            if (is_reg2) {
                if (cpu->registers[reg2] != 0) {
                    cpu->registers[reg1] /= cpu->registers[reg2];
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Division by zero!\n");
                    cpu->running = 0;
                }
            } else if (mode == 3) {
                if (value != 0) {
                    cpu->registers[reg1] /= value;
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Division by zero!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid DIV mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 7:
            if (is_reg2) {
                if (cpu->registers[reg2] != 0) {
                    cpu->registers[reg1] %= cpu->registers[reg2];
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Division by zero in MOD!\n");
                    cpu->running = 0;
                }
            } else if (mode == 3) {
                if (value != 0) {
                    cpu->registers[reg1] %= value;
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Division by zero in MOD!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid MOD mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 8:
            if (is_reg2) {
                cpu->registers[reg1] &= cpu->registers[reg2];
            } else if (mode == 3) {
                cpu->registers[reg1] &= value;
            } else {
                printf("Error: Invalid AND mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 9:
            if (is_reg2) {
                cpu->registers[reg1] |= cpu->registers[reg2];
            } else if (mode == 3) {
                cpu->registers[reg1] |= value;
            } else {
                printf("Error: Invalid OR mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 10:
            if (is_reg2) {
                cpu->registers[reg1] ^= cpu->registers[reg2];
            } else if (mode == 3) {
                cpu->registers[reg1] ^= value;
            } else {
                printf("Error: Invalid XOR mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 11:
            if (mode == 1) {
                cpu->registers[reg1] = ~cpu->registers[reg1];
                cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            } else {
                printf("Error: Invalid NOT mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 12:
            if (mode == 1) {
                cpu->registers[reg1] = -cpu->registers[reg1];
                cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            } else {
                printf("Error: Invalid NEG mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 13:
            if (is_reg2) {
                if (cpu->registers[reg2] < 16) {
                    cpu->carry_flag = (cpu->registers[reg2] > 0 && (cpu->registers[reg1] & (1 << (16 - cpu->registers[reg2])))) ? 1 : 0;
                    cpu->registers[reg1] <<= cpu->registers[reg2];
                } else {
                    cpu->carry_flag = 1;
                    cpu->registers[reg1] = 0;
                }
            } else if (mode == 3) {
                if (value < 16) {
                    cpu->carry_flag = (value > 0 && (cpu->registers[reg1] & (1 << (16 - value)))) ? 1 : 0;
                    cpu->registers[reg1] <<= value;
                } else {
                    cpu->carry_flag = 1;
                    cpu->registers[reg1] = 0;
                }
            } else {
                printf("Error: Invalid SHL mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 14:
            if (is_reg2) {
                if (cpu->registers[reg2] < 16 && cpu->registers[reg2] > 0) {
                    cpu->carry_flag = (cpu->registers[reg1] & (1 << (cpu->registers[reg2] - 1))) ? 1 : 0;
                    cpu->registers[reg1] >>= cpu->registers[reg2];
                } else if (cpu->registers[reg2] >= 16) {
                    cpu->carry_flag = 1;
                    cpu->registers[reg1] = 0;
                }
            } else if (mode == 3) {
                if (value < 16 && value > 0) {
                    cpu->carry_flag = (cpu->registers[reg1] & (1 << (value - 1))) ? 1 : 0;
                    cpu->registers[reg1] >>= value;
                } else if (value >= 16) {
                    cpu->carry_flag = 1;
                    cpu->registers[reg1] = 0;
                }
            } else {
                printf("Error: Invalid SHR mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
            cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
            break;
        case 15:
            if (is_reg2) {
                uint32_t result = cpu->registers[reg1] - cpu->registers[reg2];
                cpu->zero_flag = (result == 0) ? 1 : 0;
                cpu->carry_flag = (cpu->registers[reg1] < cpu->registers[reg2]) ? 1 : 0;
                cpu->sign_flag = (result & 0x8000) ? 1 : 0;
            } else if (mode == 3) {
                uint32_t result = cpu->registers[reg1] - value;
                cpu->zero_flag = (result == 0) ? 1 : 0;
                cpu->carry_flag = (cpu->registers[reg1] < value) ? 1 : 0;
                cpu->sign_flag = (result & 0x8000) ? 1 : 0;
            } else {
                printf("Error: Invalid CMP mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 16:
            if (mode == 1) {
                if (cpu->sp > cpu->memory_size) {
                    cpu->sp--;
                    cpu->memory[cpu->sp] = cpu->registers[reg1];
                } else {
                    printf("Error: Stack overflow!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid PUSH mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 17:
            if (mode == 1) {
                if (cpu->sp < cpu->memory_size + cpu->stack_size) {
                    cpu->registers[reg1] = cpu->memory[cpu->sp];
                    cpu->sp++;
                } else {
                    printf("Error: Stack empty!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid POP mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 18:
            if (mode == 0) {
                if (cpu->sp > cpu->memory_size + NUM_REGISTERS - 1) {
                    for (int i = NUM_REGISTERS - 1; i >= 0; i--) {
                        cpu->sp--;
                        cpu->memory[cpu->sp] = cpu->registers[i];
                    }
                } else {
                    printf("Error: Stack overflow on PUSHA!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid PUSHA mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 19:
            if (mode == 0) {
                if (cpu->sp < cpu->memory_size + cpu->stack_size - NUM_REGISTERS) {
                    for (int i = 0; i < NUM_REGISTERS; i++) {
                        cpu->registers[i] = cpu->memory[cpu->sp];
                        cpu->sp++;
                    }
                } else {
                    printf("Error: Stack empty on POPA!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid POPA mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 20:
            if (mode == 5) {
                cpu->interrupt = value;
            } else {
                printf("Error: Invalid INT mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 21:
            if (mode == 5 || mode == 4) {
                uint16_t target_pc = value / sizeof(uint16_t);
                if (target_pc < cpu->program_size) {
                    cpu->pc = target_pc;
                } else {
                    printf("Error: JMP address %u (PC %u) out of bounds!\n", value, target_pc);
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid JMP mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 22:
            if (mode == 5 || mode == 4) {
                if (cpu->sp > cpu->memory_size) {
                    cpu->sp--;
                    cpu->memory[cpu->sp] = cpu->pc;
                    uint16_t target_pc = value / sizeof(uint16_t);
                    if (target_pc < cpu->program_size) {
                        cpu->pc = target_pc;
                    } else {
                        printf("Error: CALL address %u (PC %u) out of bounds!\n", value, target_pc);
                        cpu->running = 0;
                    }
                } else {
                    printf("Error: Stack overflow on CALL!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid CALL mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 23:
            if (mode == 0) {
                if (cpu->sp < cpu->memory_size + cpu->stack_size) {
                    cpu->pc = cpu->memory[cpu->sp];
                    cpu->sp++;
                } else {
                    printf("Error: Stack empty on RET!\n");
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid RET mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 24:
            if (mode == 5 || mode == 4) {
                if (cpu->zero_flag) {
                    uint16_t target_pc = value / sizeof(uint16_t);
                    if (target_pc < cpu->program_size) {
                        cpu->pc = target_pc;
                    } else {
                        printf("Error: JZ address %u (PC %u) out of bounds!\n", value, target_pc);
                        cpu->running = 0;
                    }
                }
            } else {
                printf("Error: Invalid JZ mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 25:
            if (mode == 5 || mode == 4) {
                if (!cpu->zero_flag) {
                    uint16_t target_pc = value / sizeof(uint16_t);
                    if (target_pc < cpu->program_size) {
                        cpu->pc = target_pc;
                    } else {
                        printf("Error: JNZ address %u (PC %u) out of bounds!\n", value, target_pc);
                        cpu->running = 0;
                    }
                }
            } else {
                printf("Error: Invalid JNZ mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 26:
            if (mode == 5 || mode == 4) {
                if (!cpu->zero_flag && !cpu->sign_flag) {
                    uint16_t target_pc = value / sizeof(uint16_t);
                    if (target_pc < cpu->program_size) {
                        cpu->pc = target_pc;
                    } else {
                        printf("Error: JG address %u (PC %u) out of bounds!\n", value, target_pc);
                        cpu->running = 0;
                    }
                }
            } else {
                printf("Error: Invalid JG mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 27:
            if (mode == 5 || mode == 4) {
                if (cpu->sign_flag) {
                    uint16_t target_pc = value / sizeof(uint16_t);
                    if (target_pc < cpu->program_size) {
                        cpu->pc = target_pc;
                    } else {
                        printf("Error: JL address %u (PC %u) out of bounds!\n", value, target_pc);
                        cpu->running = 0;
                    }
                }
            } else {
                printf("Error: Invalid JL mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 28:
            if (mode == 6) {
                if (value < cpu->memory_size * sizeof(uint16_t)) {
                    cpu->registers[reg1] = cpu->memory[value / sizeof(uint16_t)];
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Memory read address 0x%04x out of bounds at PC %u!\n", value, cpu->pc - 1);
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid MOV reg,[mem] mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        case 29:
            if (mode == 7) {
                if (value < cpu->memory_size * sizeof(uint16_t)) {
                    cpu->memory[value / sizeof(uint16_t)] = cpu->registers[reg1];
                    cpu->zero_flag = (cpu->registers[reg1] == 0) ? 1 : 0;
                    cpu->sign_flag = (cpu->registers[reg1] & 0x8000) ? 1 : 0;
                } else {
                    printf("Error: Memory write address 0x%04x out of bounds at PC %u!\n", value, cpu->pc - 1);
                    cpu->running = 0;
                }
            } else {
                printf("Error: Invalid MOV [mem],reg mode %u at PC %u!\n", mode, cpu->pc - 1);
                cpu->running = 0;
            }
            break;
        default:
            printf("Error: Unknown opcode %u at PC %u!\n", opcode, cpu->pc - 1);
            cpu->running = 0;
            break;
    }
}

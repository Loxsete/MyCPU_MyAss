# CPU System Documentation

## Overview

This system is a 16-bit CPU emulator with its own assembler and BIOS. The system consists of three main components:

- **Assembler** (`assembler.c`) - translates assembly code into binary files
- **CPU** (`cpu.c`) - emulates a 16-bit processor with instruction set
- **BIOS** (`bios.c`) - provides system calls and I/O management

## CPU Architecture

### Registers

The system has 7 16-bit registers:

- `AX` (R0) - Accumulator
- `BX` (R1) - Base register  
- `CX` (R2) - Counter
- `DX` (R3) - Data register
- `SP` (R4) - Stack pointer
- `BP` (R5) - Base pointer
- `IP` (R6) - Instruction pointer

### Flags

- `zero_flag` - set when result is zero
- `carry_flag` - set on overflow
- `sign_flag` - set for negative results

### Memory

- Memory organized as array of 16-bit words
- Byte addressing (addresses in bytes)
- Stack grows downward from top of memory
- Programs loaded at address 0x1000 (4096 bytes)

## Instruction Set

### Basic Instructions

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 0 | NOP | No operation |
| 1 | HLT | Halt execution |

### Data Movement

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 2 | MOV | Move data |
| 28 | MOV reg, [mem] | Load from memory |
| 29 | MOV [mem], reg | Store to memory |

### Arithmetic Operations

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 3 | ADD | Addition |
| 4 | SUB | Subtraction |
| 5 | MUL | Multiplication |
| 6 | DIV | Division |
| 7 | MOD | Modulo |
| 12 | NEG | Negation |

### Logical Operations

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 8 | AND | Logical AND |
| 9 | OR | Logical OR |
| 10 | XOR | Exclusive OR |
| 11 | NOT | Logical NOT |

### Shifts

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 13 | SHL | Shift left |
| 14 | SHR | Shift right |

### Comparison

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 15 | CMP | Compare |

### Stack Operations

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 16 | PUSH | Push to stack |
| 17 | POP | Pop from stack |
| 18 | PUSHA | Push all registers |
| 19 | POPA | Pop all registers |

### Control Flow

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 21 | JMP | Unconditional jump |
| 22 | CALL | Call subroutine |
| 23 | RET | Return from subroutine |
| 24 | JZ/JE | Jump if zero/equal |
| 25 | JNZ/JNE | Jump if not zero/not equal |
| 26 | JG | Jump if greater |
| 27 | JL | Jump if less |

### System Calls

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 20 | INT | Interrupt/system call |

## Addressing Modes

Instructions are encoded in the following format:
```
[5-bit opcode][3-bit reg1][3-bit reg2][5-bit mode]
```

Modes:
- 0: No operands
- 1: Single register
- 2: Register-register
- 3: Register-immediate
- 4: Register-memory
- 5: Immediate only
- 6: Memory load
- 7: Memory store

## BIOS - System Calls

### INT 1 - Keyboard Input

**Function 0x01 (GET_KEY)**
- Input: AX = 0x01
- Output: AX = key code, ZF = 0 if key pressed

**Function 0x02 (PEEK_KEY)**
- Input: AX = 0x02
- Output: AX = key code without waiting

**Function 0x03 (READ_LINE)**
- Input: AX = 0x03, BX = buffer address
- Output: String in buffer at address BX

### INT 2 - String Output

- Input: AX = string address
- Action: Outputs null-terminated string

### INT 3 - Screen Functions

**Function 0x01 - New Line**
- Input: AX = 0x01
- Action: Adds newline character to output

**Function 0x02 - Clear Screen**
- Input: AX = 0x02
- Action: Clears output buffer

### INT 4 - Delay

- Input: AX = delay time in milliseconds
- Action: Suspends execution for specified time

### INT 6 - Program Loading

- Input: AX = file index
- Action: Loads .bin file from bin/ directory

### INT 9 - Compatibility (READ_LINE)

- Action: Reads line to address 100
- Output: AX = 100 if line read

## Assembler

### Syntax

**Labels:**
```assembly
label:
    mov ax, 10
```

**Comments:**
```assembly
; This is a comment
mov ax, 10  ; End of line comment
```

**Numbers:**
```assembly
mov ax, 42      ; Decimal
mov ax, 0x2A    ; Hexadecimal
mov ax, 0b101010 ; Binary
```

### Directives

**`.org address`** - Sets program start address
```assembly
.org 0x1000
```

**Data:**
```assembly
msg: db "Hello World", 0    ; Bytes
numbers: dw 1, 2, 3, 4      ; Words (16-bit)
values: dd 0x12345678       ; Double words (32-bit)
```

### Example Program

```assembly
.org 0x1000

start:
    mov ax, msg      ; Load string address
    int 2            ; Output string
    
    mov ax, 0x01     ; New line function
    int 3            ; Call screen function
    
    hlt              ; Stop execution

msg: db "Hello, World!", 0
```

## Compilation and Execution

1. **Compile assembler:**
```bash
gcc -o assembler assembler.c
```

2. **Assemble program:**
```bash
./assembler program.asm program.bin
```

3. **Run emulator:**
```bash
gcc -o emulator main.c cpu.c bios.c -lraylib
./emulator
```

## Limitations

- Memory: 16-bit addressing (64KB)
- Registers: 7 16-bit registers
- Stack: Fixed size
- Forbidden addresses: 0xFF00-0xFFFF (reserved for BIOS/MMIO)
- Max files: defined by MAX_FILES constant
- Input buffer size: 256 characters

## Extensions

The system supports extensions through:
- New system calls (INT)
- Additional processor instructions
- Extended BIOS functionality

The system is designed for educational purposes and demonstration of basic processor and assembler principles.

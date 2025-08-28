# Corx16 Emulator

A 16-bit CPU emulator with BIOS, disk storage, and graphical interface built with Raylib.

## Overview

The Corx16 Emulator simulates a custom 16-bit processor with its own instruction set, BIOS system, virtual disk storage, and terminal-like interface. Programs are loaded from `.bin` files and executed in a controlled environment with interrupt-based system calls.

## Architecture

### CPU Module (`cpu.h`, `cpu.c`)

The CPU implements a 16-bit processor with the following features:

#### Registers and Memory
- **4 general-purpose registers** (R0-R3): 16-bit each
- **Program Counter (PC)**: Points to current instruction
- **Stack Pointer (SP)**: Manages call stack
- **Memory**: Configurable size (default 4096 words + 1024 stack words)
- **Flags**: Zero, Carry, and Sign flags for conditional operations

#### Instruction Set Architecture

Instructions use a 16-bit encoding format:
- **Opcode**: 5 bits (bits 15-11)
- **Register 1**: 3 bits (bits 10-8)  
- **Mode/Register 2**: 5 bits (bits 4-0)

**Supported Instructions:**

| Opcode | Mnemonic | Description |
|--------|----------|-------------|
| 0 | NOP | No operation |
| 1 | HLT | Halt execution |
| 2 | MOV | Move data between registers/memory |
| 3 | ADD | Addition |
| 4 | SUB | Subtraction |
| 5 | MUL | Multiplication |
| 6 | DIV | Division |
| 7 | MOD | Modulo |
| 8 | AND | Bitwise AND |
| 9 | OR | Bitwise OR |
| 10 | XOR | Bitwise XOR |
| 11 | NOT | Bitwise NOT |
| 12 | NEG | Two's complement negation |
| 13 | SHL | Shift left |
| 14 | SHR | Shift right |
| 15 | CMP | Compare (sets flags) |
| 16 | PUSH | Push register to stack |
| 17 | POP | Pop from stack to register |
| 18 | PUSHA | Push all registers |
| 19 | POPA | Pop all registers |
| 20 | INT | Software interrupt |
| 21 | JMP | Unconditional jump |
| 22 | CALL | Call subroutine |
| 23 | RET | Return from subroutine |
| 24 | JZ | Jump if zero |
| 25 | JNZ | Jump if not zero |
| 26 | JG | Jump if greater |
| 27 | JL | Jump if less |
| 28 | MOV | Load from memory to register |
| 29 | MOV | Store register to memory |

#### Addressing Modes
- **Mode 0**: No operands
- **Mode 1**: Single register
- **Mode 2**: Register-to-register
- **Mode 3**: Register with immediate value
- **Mode 4**: Memory addressing
- **Mode 5**: Immediate addressing
- **Mode 6**: Load from memory `[addr]` to register
- **Mode 7**: Store register to memory `[addr]`

### BIOS Module (`bios.h`, `bios.c`)

The BIOS provides system services through software interrupts and manages the boot process.

#### Features
- **File System**: Scans `bin/` directory for `.bin` files
- **Boot Menu**: Interactive file selection interface
- **Command History**: Up to 50 previous commands
- **Input Handling**: Keyboard input with line editing
- **Program Execution**: Loads and manages program execution

#### System Interrupts

| Interrupt | Function | Description |
|-----------|----------|-------------|
| 1 | Keyboard Input | Various keyboard input modes |
| 2 | Print String | Output null-terminated string |
| 3 | Output Control | Control output formatting |
| 4 | Delay | Wait for specified milliseconds |
| 6 | Load Program | Load program by file index |
| 9 | Read Line | Read line with command history |
| 10 | Disk Operations | File system operations |

#### Keyboard Input (INT 1)
- **Function 0x01**: Single key press detection
- **Function 0x02**: Key hold detection  
- **Function 0x03**: Read complete line input

#### Output Control (INT 3)
- **Function 0x01**: Append newline to output
- **Function 0x02**: Clear output buffer

#### Disk Operations (INT 10)
- **Function 0x01**: Read from disk
- **Function 0x02**: Write to disk
- **Function 0x03**: Get disk status
- **Function 0x04**: Create file
- **Function 0x05**: Delete file

### Disk Module (`disk.h`, `disk.c`)

Implements a virtual disk storage system with file operations.

#### Features
- **Disk Size**: 1MB virtual disk (`disk.img`)
- **File System**: Simple file allocation table
- **Buffered I/O**: 4KB buffer with dirty bit tracking
- **File Operations**: Create, delete, read, write files
- **Persistence**: Automatically saves changes to disk image

#### Internal Structure
- **Directory Offset**: 1024 bytes
- **File Entries**: Up to 128 files, 64 bytes per entry
- **File Entry**: Name (64 chars), start address, size

### Window Module (`window.h`, `window.c`)

Provides the graphical interface using Raylib.

#### Features
- **Fullscreen Display**: Adapts to screen resolution
- **Font Support**: TrueType font rendering
- **Boot Menu**: File selection interface
- **Program Output**: Terminal-like text display
- **Input Visualization**: Shows current input with cursor

#### Interface Modes
1. **Boot Screen**: Shows available `.bin` files
2. **Execution Mode**: Displays program output and input prompt

## Usage

### Building
Ensure you have Raylib installed, then compile:
```bash
gcc -o emulator emulator.c cpu.c bios.c disk.c window.c -lraylib -lm
```

### Running
```bash
./emulator [memory_size] [stack_size]
```
- `memory_size`: Memory size in words (default: 4096)
- `stack_size`: Stack size in words (default: 1024)

### Creating Programs
1. Write assembly code using the Corx16 instruction set
2. Assemble to `.bin` format
3. Place in `bin/` directory
4. Programs should use `.org 0x1000` directive for proper loading

### Program Structure
```assembly
.org 0x1000        ; Standard load address

start:
    mov r0, hello   ; Load string address
    int 2           ; Print string
    hlt             ; Halt execution

hello:
    .ascii "Hello, World!", 0
```

### System Calls Example
```assembly
; Print string
mov r0, msg_addr
int 2

; Read user input
mov r0, 0x03      ; Read line function
mov r1, buffer    ; Buffer address  
int 1

; Wait 1 second
mov r0, 1000      ; 1000 milliseconds
int 4
```

## Controls

### Boot Menu
- **Up/Down Arrow**: Navigate file list
- **Enter**: Boot selected program
- **Q**: Return to boot menu (during execution)

### Program Execution
- **Standard Input**: Type text and press Enter
- **Up/Down Arrow**: Navigate command history
- **Backspace**: Edit input
- **Q**: Quit current program

## File System

The emulator creates a `disk.img` file for persistent storage. Programs can create, read, write, and delete files using disk interrupt calls.

## Error Handling

The emulator includes comprehensive error checking:
- Memory bounds validation
- Stack overflow/underflow protection
- Invalid instruction detection
- Disk operation error reporting
- File system integrity checks

## Limitations

- Maximum 128 files in file system
- 1MB virtual disk capacity
- 16-bit address space (64KB)
- No floating-point operations
- Single-threaded execution model

## Dependencies

- **Raylib**: Graphics and input handling
- **Standard C Libraries**: File I/O, memory management
- **POSIX**: Directory scanning (`dirent.h`)

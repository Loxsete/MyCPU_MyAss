# Compiler and flags
CC = gcc
CFLAGS = -Wall -Iinclude
LDFLAGS = -lraylib

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/emulator.c $(SRC_DIR)/cpu.c $(SRC_DIR)/bios.c $(SRC_DIR)/window.c $(SRC_DIR)/disk.c
ASSEMBLER_SRC = $(SRC_DIR)/assembler.c

# Object files
OBJS = $(BIN_DIR)/emulator.o $(BIN_DIR)/cpu.o $(BIN_DIR)/bios.o $(BIN_DIR)/window.o $(BIN_DIR)/disk.o
ASSEMBLER_OBJ = $(BIN_DIR)/assembler.o

# Output binaries
EMULATOR = emulator
ASSEMBLER = assembler

# Default target
all: $(BIN_DIR) $(EMULATOR) $(ASSEMBLER)

# Create bin directory
$(BIN_DIR):
		mkdir -p $(BIN_DIR)

# Link emulator
$(EMULATOR): $(OBJS)
		$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Link assembler
$(ASSEMBLER): $(ASSEMBLER_OBJ)
		$(CC) -o $@ $(ASSEMBLER_OBJ)

# Compile source files to object files
$(BIN_DIR)/emulator.o: $(SRC_DIR)/emulator.c $(INCLUDE_DIR)/cpu.h $(INCLUDE_DIR)/bios.h $(INCLUDE_DIR)/window.h
		$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/cpu.o: $(SRC_DIR)/cpu.c $(INCLUDE_DIR)/cpu.h
		$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/bios.o: $(SRC_DIR)/bios.c $(INCLUDE_DIR)/bios.h $(INCLUDE_DIR)/cpu.h $(INCLUDE_DIR)/disk.h
		$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/window.o: $(SRC_DIR)/window.c $(INCLUDE_DIR)/window.h $(INCLUDE_DIR)/bios.h $(INCLUDE_DIR)/cpu.h
		$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/disk.o: $(SRC_DIR)/disk.c $(INCLUDE_DIR)/disk.h
		$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/assembler.o: $(SRC_DIR)/assembler.c
		$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
		rm -rf $(BIN_DIR)/*.o $(EMULATOR) $(ASSEMBLER)

.PHONY: all clean

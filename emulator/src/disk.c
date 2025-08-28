#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 4096
#define MAX_FILES 128
#define FILE_ENTRY_SIZE 64
#define DIR_OFFSET 1024

typedef struct {
    char name[MAX_FILENAME];
    uint32_t start_addr;
    uint32_t size;
} FileEntry;

struct Disk {
    FILE* file;
    uint16_t last_error;
    uint8_t buffer[BUFFER_SIZE];
    uint32_t buffer_addr;
    int buffer_dirty;
    FileEntry files[MAX_FILES];
    int file_count;
};

Disk* disk_init(void) {
    Disk* disk = (Disk*)calloc(1, sizeof(Disk));
    if (!disk) {
        fprintf(stderr, "Error: Failed to allocate memory for disk!\n");
        exit(1);
    }

    disk->file = fopen(DISK_FILE, "r+b");
    if (!disk->file) {
        disk->file = fopen(DISK_FILE, "w+b");
        if (!disk->file) {
            fprintf(stderr, "Error: Failed to create disk.img: %s\n", strerror(errno));
            free(disk);
            exit(1);
        }
        uint8_t zero = 0;
        for (size_t i = 0; i < DISK_SIZE; i++) {
            fwrite(&zero, 1, 1, disk->file);
        }
    }

    // Load file system
    fseek(disk->file, DIR_OFFSET, SEEK_SET);
    fread(disk->files, sizeof(FileEntry), MAX_FILES, disk->file);
    for (int i = 0; i < MAX_FILES; i++) {
        if (disk->files[i].name[0] != '\0') {
            disk->file_count++;
        }
    }

    disk->buffer_addr = (uint32_t)-1;
    disk->buffer_dirty = 0;
    return disk;
}

void disk_flush_buffer(Disk* disk) {
    if (disk->buffer_dirty && disk->buffer_addr != (uint32_t)-1) {
        fseek(disk->file, disk->buffer_addr, SEEK_SET);
        fwrite(disk->buffer, 1, BUFFER_SIZE, disk->file);
        disk->buffer_dirty = 0;
    }
}

void disk_cleanup(Disk* disk) {
    disk_flush_buffer(disk);
    if (disk->file) {
        fseek(disk->file, DIR_OFFSET, SEEK_SET);
        fwrite(disk->files, sizeof(FileEntry), MAX_FILES, disk->file);
        fclose(disk->file);
    }
    free(disk);
}

int disk_read(Disk* disk, uint32_t addr, size_t len, uint8_t* data) {
    if (addr + len > DISK_SIZE) {
        disk->last_error = 1;
        fprintf(stderr, "Disk read error: Address 0x%04X + %zu exceeds disk size\n", addr, len);
        return 1;
    }

    size_t offset = 0;
    while (offset < len) {
        uint32_t block_addr = addr + offset - (addr + offset) % BUFFER_SIZE;
        size_t block_offset = (addr + offset) % BUFFER_SIZE;
        size_t to_read = len - offset < BUFFER_SIZE - block_offset ? len - offset : BUFFER_SIZE - block_offset;

        if (block_addr != disk->buffer_addr) {
            disk_flush_buffer(disk);
            fseek(disk->file, block_addr, SEEK_SET);
            size_t read = fread(disk->buffer, 1, BUFFER_SIZE, disk->file);
            if (read != BUFFER_SIZE && !feof(disk->file)) {
                disk->last_error = 1;
                fprintf(stderr, "Disk read error: Failed to read buffer at 0x%04X\n", block_addr);
                return 1;
            }
            disk->buffer_addr = block_addr;
        }

        memcpy(data + offset, disk->buffer + block_offset, to_read);
        offset += to_read;
    }

    disk->last_error = 0;
    printf("Disk read: addr=0x%04X, len=%zu\n", addr, len);
    return 0;
}

int disk_write(Disk* disk, uint32_t addr, size_t len, const uint8_t* data) {
    if (addr + len > DISK_SIZE) {
        disk->last_error = 1;
        fprintf(stderr, "Disk write error: Address 0x%04X + %zu exceeds disk size\n", addr, len);
        return 1;
    }

    size_t offset = 0;
    while (offset < len) {
        uint32_t block_addr = addr + offset - (addr + offset) % BUFFER_SIZE;
        size_t block_offset = (addr + offset) % BUFFER_SIZE;
        size_t to_write = len - offset < BUFFER_SIZE - block_offset ? len - offset : BUFFER_SIZE - block_offset;

        if (block_addr != disk->buffer_addr) {
            disk_flush_buffer(disk);
            fseek(disk->file, block_addr, SEEK_SET);
            size_t read = fread(disk->buffer, 1, BUFFER_SIZE, disk->file);
            if (read != BUFFER_SIZE && !feof(disk->file)) {
                disk->last_error = 1;
                fprintf(stderr, "Disk read error: Failed to read buffer at 0x%04X\n", block_addr);
                return 1;
            }
            disk->buffer_addr = block_addr;
        }

        memcpy(disk->buffer + block_offset, data + offset, to_write);
        disk->buffer_dirty = 1;
        offset += to_write;
    }

    disk->last_error = 0;
    printf("Disk write: addr=0x%04X, len=%zu\n", addr, len);
    return 0;
}

uint16_t disk_status(Disk* disk) {
    return disk->last_error;
}

int disk_create_file(Disk* disk, const char* filename) {
    if (disk->file_count >= MAX_FILES) {
        disk->last_error = 2;
        fprintf(stderr, "Disk error: Max file limit reached\n");
        return 1;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (disk->files[i].name[0] == '\0') {
            strncpy(disk->files[i].name, filename, MAX_FILENAME - 1);
            disk->files[i].name[MAX_FILENAME - 1] = '\0';
            disk->files[i].start_addr = DIR_OFFSET + MAX_FILES * FILE_ENTRY_SIZE + disk->file_count * 1024;
            disk->files[i].size = 0;
            disk->file_count++;
            disk->last_error = 0;
            return 0;
        }
    }

    disk->last_error = 2;
    fprintf(stderr, "Disk error: No free file slots\n");
    return 1;
}

int disk_delete_file(Disk* disk, const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(disk->files[i].name, filename) == 0) {
            disk->files[i].name[0] = '\0';
            disk->files[i].start_addr = 0;
            disk->files[i].size = 0;
            disk->file_count--;
            disk->last_error = 0;
            return 0;
        }
    }

    disk->last_error = 3;
    fprintf(stderr, "Disk error: File %s not found\n", filename);
    return 1;
}

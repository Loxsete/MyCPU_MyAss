#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DISK_SIZE 1048576  // 1 MB
#define DISK_FILE "disk.img"

Disk* disk_init(void) {
    Disk* disk = (Disk*)malloc(sizeof(Disk));
    if (!disk) {
        printf("Error: Failed to allocate memory for disk!\n");
        exit(1);
    }
    disk->last_error = 0;

    FILE* file = fopen(DISK_FILE, "r+b");
    if (!file) {
        file = fopen(DISK_FILE, "w+b");
        if (!file) {
            printf("Error: Failed to create disk.img: %s\n", strerror(errno));
            free(disk);
            exit(1);
        }
        uint8_t zero = 0;
        for (size_t i = 0; i < DISK_SIZE; i++) {
            fwrite(&zero, 1, 1, file);
        }
    }
    disk->file = file;
    return disk;
}

void disk_cleanup(Disk* disk) {
    if (disk->file) {
        fclose(disk->file);
    }
    free(disk);
}

int disk_read(Disk* disk, uint32_t addr, size_t len, uint8_t* data) {
    if (addr + len > DISK_SIZE) {
        disk->last_error = 1;
        printf("Disk read error: Address 0x%04X + %zu exceeds disk size\n", addr, len);
        return 1;
    }
    fseek(disk->file, addr, SEEK_SET);
    size_t read = fread(data, 1, len, disk->file);
    if (read != len) {
        disk->last_error = 1;
        printf("Disk read error: Failed to read %zu bytes at 0x%04X\n", len, addr);
        return 1;
    }
    disk->last_error = 0;
    printf("Disk read: addr=0x%04X, len=%zu, data=%s\n", addr, len, data);
    return 0;
}

int disk_write(Disk* disk, uint32_t addr, size_t len, const uint8_t* data) {
    if (addr + len > DISK_SIZE) {
        disk->last_error = 1;
        printf("Disk write error: Address 0x%04X + %zu exceeds disk size\n", addr, len);
        return 1;
    }
    fseek(disk->file, addr, SEEK_SET);
    size_t written = fwrite(data, 1, len, disk->file);
    if (written != len) {
        disk->last_error = 1;
        printf("Disk write error: Failed to write %zu bytes at 0x%04X\n", len, addr);
        return 1;
    }
    fflush(disk->file); 
    disk->last_error = 0;
    printf("Disk write: addr=0x%04X, len=%zu, data=%s\n", addr, len, data);
    return 0;
}

uint16_t disk_status(Disk* disk) {
    return disk->last_error;
}

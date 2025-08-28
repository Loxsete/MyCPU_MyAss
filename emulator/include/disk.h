#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#include <stdio.h>

#define DISK_SIZE 1048576  // 1 MB
#define DISK_FILE "disk.img"
#define MAX_FILENAME 64

typedef struct Disk Disk;

Disk* disk_init(void);
void disk_cleanup(Disk* disk);
int disk_read(Disk* disk, uint32_t addr, size_t len, uint8_t* data);
int disk_write(Disk* disk, uint32_t addr, size_t len, const uint8_t* data);
uint16_t disk_status(Disk* disk);
int disk_create_file(Disk* disk, const char* filename);
int disk_delete_file(Disk* disk, const char* filename);

#endif

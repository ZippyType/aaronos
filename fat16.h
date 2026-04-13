#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

#define PACKED __attribute__((packed))

struct FAT16_BPB {
    uint8_t  jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} PACKED;

struct FAT16_DirEntry {
    char     filename[8];
    char     extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high; 
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} PACKED;

void fat16_format_drive();
void fat16_list_files();
void fat16_cat(char* name);
void fat16_write_to_test(char* content);
void fat16_create_file(char* name);
void fat16_delete_file(char* name);
void fat16_rename_file(char* oldname, char* newname);

#endif
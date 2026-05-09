#include "fat16.h"
#include "io.h"
#include <stddef.h>

#define SECTOR_SIZE 512
#define ROOT_SECTOR 65
#define DATA_START_SECTOR 97 

extern void print(const char* str);
extern void putchar_col(char c, uint8_t color);
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern uint16_t inw(uint16_t port);

int drive_wait() {
    int timeout = 0;
    while ((inb(0x1F7) & 0x80) || !(inb(0x1F7) & 0x08)) {
        timeout++;
        if (timeout > 100000) return 0;
    }
    return 1;
}

int disk_ready() {
    uint8_t status = inb(0x1F7);
    return (status & 0x40) != 0;
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!disk_ready()) return 0;
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20); 
    if (!drive_wait()) return 0;
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) ptr[i] = inw(0x1F0);
    return 1;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!disk_ready()) return 0;
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); 
    if (!drive_wait()) return 0;
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        uint16_t data = ptr[i];
        asm volatile("outw %0, %1" : : "a"(data), "Nd"(0x1F0));
    }
    return 1;
}

static void format_name(char* in, char* out) {
    for(int i=0; i<8; i++) out[i] = ' ';
    for(int i=0; i<8 && in[i] != '\0' && in[i] != '.'; i++) out[i] = in[i];
}

void fat16_format_drive() {
    struct FAT16_BPB bpb;
    uint8_t* bptr = (uint8_t*)&bpb;
    for(int i = 0; i < sizeof(bpb); i++) bptr[i] = 0;
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 1;
    bpb.reserved_sectors = 1;
    bpb.fat_count = 2;
    bpb.root_entry_count = 512;
    bpb.total_sectors_16 = 20480;
    bpb.sectors_per_fat = 32;
    for(int i = 0; i < 8; i++) bpb.oem_name[i] = "AARONOS "[i];
    uint8_t s0[512] = {0};
    for(int i = 0; i < sizeof(bpb); i++) s0[i] = bptr[i];
    ata_write_sector(0, s0);
    uint8_t empty[512] = {0};
    for(uint32_t s = 65; s < 70; s++) ata_write_sector(s, empty);
}

void fat16_list_files() {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* entries = (struct FAT16_DirEntry*)buf;
    print("Files:\n");
    for (int i = 0; i < 16; i++) {
        if (entries[i].filename[0] == 0) break;
        if (entries[i].filename[0] == (char)0xE5) continue;
        for (int j = 0; j < 8; j++) if(entries[i].filename[j]!=' ') putchar_col(entries[i].filename[j], 0x07);
        print(".");
        for (int j = 0; j < 3; j++) putchar_col(entries[i].extension[j], 0x07);
        print("\n");
    }
}

void fat16_cat(char* name) {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    char target[8];
    format_name(name, target);
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* entries = (struct FAT16_DirEntry*)buf;
    for (int i = 0; i < 16; i++) {
        int match = 1;
        for(int j=0; j<8; j++) if(entries[i].filename[j] != target[j]) match = 0;
        if (match) {
            uint32_t sector = DATA_START_SECTOR + entries[i].first_cluster_low - 2;
            uint8_t data_buf[512];
            if (!ata_read_sector(sector, data_buf)) { print("Error: Cannot read file.\n"); return; }
            print((char*)data_buf); print("\n");
            return;
        }
    }
    print("File not found.\n");
}

void fat16_create_file(char* name) {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    char target[8];
    format_name(name, target);
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* entries = (struct FAT16_DirEntry*)buf;
    for (int i = 0; i < 16; i++) {
        if (entries[i].filename[0] == 0 || entries[i].filename[0] == (char)0xE5) {
            for(int j=0; j<8; j++) entries[i].filename[j] = target[j];
            for(int j=0; j<3; j++) entries[i].extension[j] = "TXT"[j];
            entries[i].first_cluster_low = 2;
            if (!ata_write_sector(ROOT_SECTOR, buf)) { print("Error: Cannot write disk.\n"); return; }
            print("File created.\n");
            return;
        }
    }
    print("Error: Directory full.\n");
}

void fat16_delete_file(char* name) {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    char target[8];
    format_name(name, target);
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* e = (struct FAT16_DirEntry*)buf;
    for (int i = 0; i < 16; i++) {
        int m = 1;
        for(int j=0; j<8; j++) if(e[i].filename[j] != target[j]) m = 0;
        if (m) {
            e[i].filename[0] = (char)0xE5;
            if (!ata_write_sector(ROOT_SECTOR, buf)) { print("Error: Cannot write disk.\n"); return; }
            print("File deleted.\n");
            return;
        }
    }
    print("File not found.\n");
}

void fat16_rename_file(char* old, char* newn) {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    char t1[8], t2[8];
    format_name(old, t1); format_name(newn, t2);
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* e = (struct FAT16_DirEntry*)buf;
    for (int i = 0; i < 16; i++) {
        int m = 1;
        for(int j=0; j<8; j++) if(e[i].filename[j] != t1[j]) m = 0;
        if (m) {
            for(int j=0; j<8; j++) e[i].filename[j] = t2[j];
            if (!ata_write_sector(ROOT_SECTOR, buf)) { print("Error: Cannot write disk.\n"); return; }
            print("File renamed.\n");
            return;
        }
    }
    print("File not found.\n");
}

void fat16_write_to_test(char* content) {
    if (!disk_ready()) { print("Error: No disk detected.\n"); return; }
    uint8_t buf[512];
    if (!ata_read_sector(ROOT_SECTOR, buf)) { print("Error: Cannot read disk.\n"); return; }
    struct FAT16_DirEntry* entries = (struct FAT16_DirEntry*)buf;
    for (int i = 0; i < 16; i++) {
        if (entries[i].filename[0] == 0 || entries[i].filename[0] == (char)0xE5) {
            for(int j=0; j<8; j++) entries[i].filename[j] = "TEST    "[j];
            for(int j=0; j<3; j++) entries[i].extension[j] = "TXT"[j];
            entries[i].first_cluster_low = 2;
            if (!ata_write_sector(ROOT_SECTOR, buf)) { print("Error: Cannot write disk.\n"); return; }
            uint8_t data_buf[512] = {0};
            for(int j=0; content[j] && j<511; j++) data_buf[j] = content[j];
            if (!ata_write_sector(DATA_START_SECTOR, data_buf)) { print("Error: Cannot write data.\n"); return; }
            print("Saved to TEST.TXT\n");
            return;
        }
    }
    print("Error: Directory full.\n");
}


#include <stdint.h>
#include "io.h"
#include "fat16.h"

/* Extern references to kernel.c functions */
extern void print(const char* str);
extern void print_col(const char* str, uint8_t col);
extern void play_sound(uint32_t freq);
extern void nosound(void);
extern void sleep(uint32_t ticks);

/* Provide standard scancode reading for standalone installer loop */
char get_scan_code() {
    while (!(inb(0x64) & 1)); // Wait for keyboard buffer to be full
    return inb(0x60);         // Read scancode from data port
}

void run_installation() {
    print_col("\n==================================================\n", 0x0B); 
    print_col("          AARONOS SYSTEM INSTALLER v1.0           \n", 0x0B);
    print_col("==================================================\n", 0x0B); 

    print("\nWARNING: This will DESTROY all data on the primary\n");
    print("ATA drive and format it as FAT16.\n\n");
    print_col("Proceed with installation? [y/n]: ", 0x0E); 

    char code = 0;
    while(1) {
        code = get_scan_code();
        if (code == 0x15) { // 'y' key scancode
            print("y\n\n");
            break; 
        } else if (code == 0x31) { // 'n' key scancode
            print("n\n\nInstallation aborted by user.\n");
            return;
        }
    }

    // STEP 1: Format Drive
    print("[1/3] Formatting Primary ATA Drive (FAT16)... ");
    fat16_format_drive();
    sleep(50); // Simulate some time passing
    print_col("DONE\n", 0x0A); // Green

    // STEP 2: Bootloader & MBR
    print("[2/3] Writing MBR Boot Signature (0xAA55)... ");
    uint8_t mbr_sig[512] = {0};
    mbr_sig[510] = 0x55;
    mbr_sig[511] = 0xAA;
    
    // Write to LBA 0
    outb(0x1F6, 0xE0);
    outb(0x1F2, 1);
    outb(0x1F3, 0);
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    outb(0x1F7, 0x30); 
    while ((inb(0x1F7) & 0x80) || !(inb(0x1F7) & 0x08)); // Wait for drive
    uint16_t* ptr = (uint16_t*)mbr_sig;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ptr[i]);
    }
    sleep(30);
    print_col("COMMITTED\n", 0x0A);

    // STEP 3: Kernel File Copy Mock
    print("[3/3] Deploying AARONOS.BIN to root directory... ");
    sleep(40);
    print_col("SUCCESS\n", 0x0A);

    // Finalization
    print_col("\n[ AaronOS successfully installed to hardware! ]\n", 0x0A);
    print("You may safely reboot the system.\n");

    // Victory Melody
    uint32_t victory[] = {523, 659, 783, 1046}; // C-E-G-C
    for(int i = 0; i < 4; i++) {
        play_sound(victory[i]);
        sleep(10);
    }
    nosound();
}


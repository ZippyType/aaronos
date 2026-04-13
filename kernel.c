/**
 * =============================================================================
 * AARONOS KERNEL - FULL MONOLITHIC BUILD 
 * =============================================================================
 * VERSION: 3.9.0-STABLE
 * ARCHITECTURE: x86 (i386)
 * DESCRIPTION: High-stability monolithic kernel with persistent storage hooks,
 * PIT-based audio engine, deep hardware monitoring, comprehensive 
 * hardware interrupt handling, and an extended utility shell.
 * =============================================================================
 */

#include <stdint.h>
#include <stddef.h>
#include "io.h"      
#include "fat16.h"   

/* ========================================================================== */
/* 1. KERNEL SYSTEM IDENTITY                                                  */
/* ========================================================================== */

#define KERNEL_NAME        "AaronOS"
#define KERNEL_VERSION     "3.9.0-STABLE"
#define KERNEL_BUILD       "2026-04-13-QEMU/UTM"

/* ========================================================================== */
/* 2. HARDWARE MEMORY & PORTS                                                 */
/* ========================================================================== */

#define VIDEO_ADDR         0xB8000
#define SCREEN_WIDTH       80
#define SCREEN_HEIGHT      25

#define PIT_CHANNEL_0      0x40
#define PIT_CHANNEL_1      0x41
#define PIT_CHANNEL_2      0x42
#define PIT_COMMAND        0x43

#define PC_SPEAKER_PORT    0x61
#define KBD_STATUS_PORT    0x64
#define KBD_DATA_PORT      0x60

#define CMOS_ADDRESS       0x70
#define CMOS_DATA          0x71

/* Base Color Palettes */
#define COLOR_DEFAULT      0x07 
#define COLOR_SUCCESS      0x0A 
#define COLOR_HELP         0x0B 
#define COLOR_ALERT        0x0E 
#define COLOR_PANIC        0x4F 
#define COLOR_AUDIO        0x0D 
#define COLOR_MATRIX       0x0A

/* Audio Frequencies */
#define NOTE_C4            261
#define NOTE_D4            294
#define NOTE_E4            329
#define NOTE_F4            349
#define NOTE_G4            392
#define NOTE_A4            440
#define NOTE_B4            493
#define NOTE_C5            523

/* TUI Visual Elements */
#define TUI_COLOR       0x1F  // White text on Blue background
#define BOX_HLINE       0xCD  // ═
#define BOX_VLINE       0xBA  // ║
#define BOX_TL          0xC9  // ╔
#define BOX_TR          0xBB  // ╗
#define BOX_BL          0xC8  // ╚
#define BOX_BR          0xBC  // ╝

#define SCROLLBACK_LIMIT 100  // Number of lines stored in RAM
#define SCREEN_HEIGHT 25
#define SCREEN_WIDTH 80

// The virtual screen: Stores character and color attribute
// 100 lines of scrollback
uint16_t terminal_buffer[100][80]; 
int scroll_offset = 0;
int current_row = 0;
int current_col = 0;
/* ========================================================================== */
/* 3. FORWARD DECLARATIONS                                                    */
/* ========================================================================== */

void nosound();
void sleep(uint32_t ticks);
void play_sound(uint32_t nFrequence);
void update_cursor();
void clear_screen();
void print(const char* str);
void print_col(const char* str, uint8_t col);
void putchar_col(char c, uint8_t color);
void putchar_at(char c, uint8_t color, int x, int y);
void print_at(const char* str, uint8_t color, int x, int y);
void kpanic(const char* message);
void sys_reboot();
void init_timer(uint32_t frequency);
void refresh_screen();
void read_rtc();
void process_shell();
void show_credits();
void run_matrix();
void print_stats();
void update_cursor_relative();

void scroll_up() {
    if (scroll_offset > 0) {
        scroll_offset--;
        refresh_screen();
    }
}

void scroll_down() {
    // 100 is your buffer limit, 25 is screen height
    if (scroll_offset < (100 - 25)) {
        // Only scroll down if we aren't past where we've actually typed
        if (scroll_offset < current_row) {
            scroll_offset++;
            refresh_screen();
        }
    }
}

/* ========================================================================== */
/* 4. GLOBAL HARDWARE & SYSTEM STATE                                          */
/* ========================================================================== */

volatile uint32_t timer_ticks = 0; 
uint8_t current_term_color = COLOR_DEFAULT; // Global terminal color

void timer_callback() {
    timer_ticks++;
}
void refresh_screen() {
    uint16_t* vga = (uint16_t*)0xB8000;
    
    for (int y = 0; y < 25; y++) { // Physical screen height
        for (int x = 0; x < 80; x++) { // Physical screen width
            
            // Calculate which line in the terminal_buffer we are looking at
            int buffer_line = y + scroll_offset;
            
            // Safety check: don't read past the end of our 100-line buffer
            if (buffer_line < 100) {
                vga[y * 80 + x] = terminal_buffer[buffer_line][x];
            } else {
                vga[y * 80 + x] = ' ' | (0x07 << 8); // Clear space if out of bounds
            }
        }
    }
    update_cursor_relative();
}

void update_cursor_relative() {
    // scroll_offset is the line at the very top of the screen
    // current_row is the line where the next character will be typed
    int visual_row = current_row - scroll_offset;

    // Only show the cursor if the typing line is actually on the screen
    if (visual_row >= 0 && visual_row < 25) {
        uint16_t pos = (visual_row * 80) + current_col;

        // Standard VGA cursor port communication
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    } else {
        // Hide the cursor by moving it off-screen if we scrolled away from the prompt
        uint16_t pos = 25 * 80; 
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

/* --- Timezone Configuration --- */
int current_offset = 2; 
char current_tz_name[32] = "Amsterdam (CEST)";

typedef struct {
    uint32_t uptime_ticks;
    uint32_t total_commands;
    uint32_t last_freq;
    uint8_t  speaker_state;
    uint8_t  disk_presence;
} kernel_health_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} rtc_time_t;

uint16_t* video_mem = (uint16_t*)VIDEO_ADDR;
int cursor_x = 0;
int cursor_y = 0;
int prompt_limit = 0;

char input_buffer[256];             
int input_ptr = 0;                  
volatile int execute_flag = 0;      
int in_gui_mode = 0; 

kernel_health_t sys_stats;
rtc_time_t system_time;

/* ========================================================================== */
/* 5. CORE STRING & MEMORY LIBRARIES                                          */
/* ========================================================================== */

int kstrcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int kstrncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void kmemset(void* dest, uint8_t val, size_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    while(len--) *ptr++ = val;
}

int katoi(const char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') { sign = -1; i++; }
    for (; str[i] >= '0' && str[i] <= '9'; ++i) {
        res = res * 10 + str[i] - '0';
    }
    return res * sign;
}

int katohex(const char* str) {
    int res = 0;
    int i = 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) i = 2;
    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            res = res * 16 + (str[i] - '0');
        } else if (str[i] >= 'a' && str[i] <= 'f') {
            res = res * 16 + (str[i] - 'a' + 10);
        } else if (str[i] >= 'A' && str[i] <= 'F') {
            res = res * 16 + (str[i] - 'A' + 10);
        } else {
            break;
        }
    }
    return res;
}

size_t kstrlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void kstrcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

char* kstrchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) {
            return NULL;
        }
    }
    return (char *)s;
}

void reverse(char str[], int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        end--;
        start++;
    }
}

void itoa(int num, char* str, int base) {
    int i = 0;
    int isNegative = 0;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (num < 0 && base == 10) {
        isNegative = 1;
        num = -num;
    }
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    if (isNegative) str[i++] = '-';
    str[i] = '\0';
    reverse(str, i);
}

/* ========================================================================== */
/* 6. REAL TIME CLOCK (CMOS) HARDWARE                                         */
/* ========================================================================== */

int get_update_in_progress_flag() {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void read_rtc() {
    uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year, registerB;
    
    while (get_update_in_progress_flag());
    system_time.second = get_rtc_register(0x00);
    system_time.minute = get_rtc_register(0x02);
    system_time.hour = get_rtc_register(0x04);
    system_time.day = get_rtc_register(0x07);
    system_time.month = get_rtc_register(0x08);
    system_time.year = get_rtc_register(0x09);

    do {
        last_second = system_time.second;
        last_minute = system_time.minute;
        last_hour = system_time.hour;
        last_day = system_time.day;
        last_month = system_time.month;
        last_year = system_time.year;

        while (get_update_in_progress_flag());
        system_time.second = get_rtc_register(0x00);
        system_time.minute = get_rtc_register(0x02);
        system_time.hour = get_rtc_register(0x04);
        system_time.day = get_rtc_register(0x07);
        system_time.month = get_rtc_register(0x08);
        system_time.year = get_rtc_register(0x09);
    } while ((last_second != system_time.second) || (last_minute != system_time.minute) || 
             (last_hour != system_time.hour) || (last_day != system_time.day) || 
             (last_month != system_time.month) || (last_year != system_time.year));

    registerB = get_rtc_register(0x0B);

    if (!(registerB & 0x04)) {
        system_time.second = (system_time.second & 0x0F) + ((system_time.second / 16) * 10);
        system_time.minute = (system_time.minute & 0x0F) + ((system_time.minute / 16) * 10);
        system_time.hour = ( (system_time.hour & 0x0F) + (((system_time.hour & 0x70) / 16) * 10) ) | (system_time.hour & 0x80);
        system_time.day = (system_time.day & 0x0F) + ((system_time.day / 16) * 10);
        system_time.month = (system_time.month & 0x0F) + ((system_time.month / 16) * 10);
        system_time.year = (system_time.year & 0x0F) + ((system_time.year / 16) * 10);
    }

    int raw_h = (int)system_time.hour;
    raw_h += current_offset;
    if (raw_h >= 24) raw_h -= 24;
    if (raw_h < 0) raw_h += 24;
    system_time.hour = (uint8_t)raw_h;

    if (!(registerB & 0x02) && (system_time.hour & 0x80)) {
        system_time.hour = ((system_time.hour & 0x7F) + 12) % 24;
    }
    system_time.year += 2000;
}

/* ========================================================================== */
/* 7. VGA TERMINAL ENGINE                                                     */
/* ========================================================================== */

void scroll() {
    if (cursor_y >= SCREEN_HEIGHT) {
        for (int i = 0; i < (SCREEN_HEIGHT - 1) * SCREEN_WIDTH; i++) {
            video_mem[i] = video_mem[i + SCREEN_WIDTH];
        }
        for (int i = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++) {
            video_mem[i] = (uint16_t)' ' | (current_term_color << 8);
        }
        cursor_y = SCREEN_HEIGHT - 1;
    }
}

void update_cursor() {
    uint16_t pos = cursor_y * SCREEN_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void putchar_col(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > prompt_limit) {
            cursor_x--;
            video_mem[cursor_y * SCREEN_WIDTH + cursor_x] = (uint16_t)' ' | (color << 8);
        }
    } else {
        video_mem[cursor_y * SCREEN_WIDTH + cursor_x] = (uint16_t)c | (color << 8);
        cursor_x++;
        if (cursor_x >= SCREEN_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    scroll();
    update_cursor();
}

void print(const char* str) {
    for (int i = 0; str[i]; i++) putchar_col(str[i], current_term_color);
}

void print_col(const char* str, uint8_t col) {
    for (int i = 0; str[i]; i++) putchar_col(str[i], col);
}

void clear_screen() {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_mem[i] = (uint16_t)' ' | (current_term_color << 8);
    }
    cursor_x = 0; cursor_y = 0;
    update_cursor();
}

void putchar_at(char c, uint8_t color, int x, int y) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        video_mem[y * SCREEN_WIDTH + x] = (uint16_t)c | (color << 8);
    }
}

void print_at(const char* str, uint8_t color, int x, int y) {
    for (int i = 0; str[i]; i++) {
        putchar_at(str[i], color, x + i, y);
    }
}

/* ========================================================================== */
/* 8. AUDIO & SOUND ENGINE                                                    */
/* ========================================================================== */

void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void play_sound(uint32_t nFrequence) {
    if (nFrequence == 0) return;
    uint32_t Div = 1193180 / nFrequence;
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CHANNEL_2, (uint8_t)(Div));
    outb(PIT_CHANNEL_2, (uint8_t)(Div >> 8));

    uint8_t tmp = inb(PC_SPEAKER_PORT);
    if (tmp != (tmp | 3)) {
        outb(PC_SPEAKER_PORT, tmp | 3);
    }
    sys_stats.last_freq = nFrequence;
    sys_stats.speaker_state = 1;
}

void nosound() {
    uint8_t tmp = inb(PC_SPEAKER_PORT) & 0xFC;
    outb(PC_SPEAKER_PORT, tmp);
    sys_stats.speaker_state = 0;
}

void sleep(uint32_t ticks) {
    uint32_t eticks = timer_ticks + ticks;
    while(timer_ticks < eticks) {
        asm volatile("hlt"); 
    }
}

void play_song(uint32_t* notes, uint32_t* durations, int length) {
    for (int i = 0; i < length; i++) {
        if (notes[i] == 0) {
            nosound();
        } else {
            play_sound(notes[i]);
        }
        sleep(durations[i]);
        nosound();
        for(volatile int d = 0; d < 500000; d++); 
    }
}

void boot_jingle() {
    print_col("[System] Initializing Audio Hardware...\n", COLOR_AUDIO);
    play_sound(523); sleep(25); 
    play_sound(659); sleep(25); 
    play_sound(783); sleep(25); 
    play_sound(1046); sleep(45); 
    nosound();
}

/* ========================================================================== */
/* 9. SYSTEM RECOVERY & DIAGNOSTICS                                           */
/* ========================================================================== */

void kpanic(const char* message) {
    kmemset(video_mem, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_mem[i] = (uint16_t)' ' | (COLOR_PANIC << 8);
    }
    cursor_x = 0; cursor_y = 0;
    print("CRITICAL_KERNEL_HALT (0xDEADBEEF)\n");
    print("The system has been halted to prevent hardware damage.\n\n");
    print("REASON: "); print(message);
    print("\n\nPress RESET on your machine to restart.");
    while(1) { asm volatile("cli; hlt"); }
}

void sys_reboot() {
    print_col("\n[ AaronOS ] System Reboot Initiated...", COLOR_ALERT);
    sleep(20);
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(KBD_STATUS_PORT);
    }
    outb(KBD_STATUS_PORT, 0xFE); 
    kpanic("REBOOT_PULSE_FAILED");
}

void print_stats() {
    char buf[16];
    print_col("--- AaronOS Engine Health ---\n", COLOR_HELP);
    print("Uptime Ticks:   "); itoa(timer_ticks, buf, 10); print(buf);
    print("\nCommands Run:   "); itoa(sys_stats.total_commands, buf, 10); print(buf);
    print("\nSpeaker Status: "); print(sys_stats.speaker_state ? "ACTIVE" : "IDLE");
    print("\nColor Pallet:   0x"); itoa(current_term_color, buf, 16); print(buf);
    print("\n");
}

/* ========================================================================== */
/* 10. VISUALS & ENTERTAINMENT                                                */
/* ========================================================================== */

void show_credits() {
    print_col("\n  _____                      ____   _____ \n", COLOR_HELP);
    print_col(" |  _  | ___  ___  ___ ___  |    | |   __|\n", COLOR_HELP);
    print_col(" |     || . ||  _|| . |   | |  |  ||__   |\n", COLOR_HELP);
    print_col(" |__|__||_  ||_|  |___|_|_| |____/ |_____|\n", COLOR_HELP);
    print_col("        |___|  BUILD: ", COLOR_HELP); 
    print_col(KERNEL_BUILD, COLOR_HELP);
    print_col("\n\n", COLOR_DEFAULT);
    print(" Lead Developer: Aaron\n");
    print(" Kernel Version: "); print(KERNEL_VERSION); print("\n");
}

void run_matrix() {
    clear_screen();
    for(int i = 0; i < 400; i++) {
        int x = (timer_ticks * 7) % SCREEN_WIDTH; 
        int y = (timer_ticks / 3) % SCREEN_HEIGHT;
        char c = (timer_ticks % 94) + 33; 
        putchar_at(c, COLOR_MATRIX, x, y);
        sleep(1);
        
        /* Cleanup fading tail effect */
        if (y > 0) putchar_at(' ', COLOR_MATRIX, x, y - 1);
    }
    clear_screen();
}

/* ========================================================================== */
/* 11. THE SHELL INTERPRETER                                                  */
/* ========================================================================== */

extern void run_installation(); 
extern void fat16_list_files();
extern void fat16_cat(char* name);
extern void fat16_write_to_test(char* content);
/* FAT16 Mock additions for advanced file ops */
extern void fat16_create_file(char* name);
extern void fat16_delete_file(char* name);
extern void fat16_rename_file(char* old_name, char* new_name);

extern void keyboard_handler_asm();
extern void timer_handler_asm();

void process_shell() {
    outb(0x20, 0x20); outb(0xA0, 0x20); 
    print("\n");
    sys_stats.total_commands++;

    if (input_ptr > 0) {
        input_buffer[input_ptr] = '\0';
        
        if (kstrcmp(input_buffer, "help") == 0) {
            print_col("--- AaronOS Command List ---\n", COLOR_HELP);
            print("install   - Run HDD deployment\n");
            print("reboot    - Warm restart\n");
            print("shutdown  - ACPI Power off\n");
            print("ver       - Show system version\n");
            print("time      - Display hardware clock\n");
            print("tz [city] - Set timezone (e.g. tz amsterdam)\n");
            print("cls       - Clear terminal window\n");
            print("panic     - Test kernel crash\n");
            print("beep [f]  - Play tone (ex: beep 440)\n");
            print("dir       - List disk contents\n");
            print("ls        - Enhanced colorized list\n");
            print("cat [f]   - Read text file\n");
            print("write [t] - Append text to disk\n");
            print("touch [f] - Create an empty file\n");
            print("rm [f]    - Delete a file\n");
            print("rename    - Rename a file (syntax: rename old new)\n");
            print("echo [t]  - Print text to screen\n");
            print("cpu       - Show hardware vendor\n");
            print("memo      - Makes a memo\n");
            print("music     - Plays a bit of music\n");
            print("siren     - Sounds a siren\n");
            print("credits   - Show OS build information\n");
            print("stats     - Show OS health and uptime\n");
            print("rand      - Generate pseudo-random number\n");
            print("matrix    - Enter the matrix\n");
            print("color [h] - Change text color (hex, e.g. color 0A)\n");
            print("calc      - Basic math (e.g. calc 5 + 10)\n");
            print("gui       - Switches to TUI (CTRL-T to return)\n");
        }
        else if (kstrcmp(input_buffer, "gui") == 0) {
            in_gui_mode = 1;
            for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
                video_mem[i] = (uint16_t)0xB1 | (0x30 << 8); 
            }
            int win_w = 60, win_h = 15;
            int start_x = (80 - win_w) / 2, start_y = (25 - win_h) / 2;
            uint8_t win_col = 0x70; 

            for(int i = 1; i < win_h; i++) {
                for(int j = 1; j < win_w; j++) {
                    putchar_at(' ', 0x08, start_x + j + 1, start_y + i + 1); 
                }
            }

            for(int i = 0; i < win_h; i++) {
                for(int j = 0; j < win_w; j++) {
                    char c = ' ';
                    if (i == 0 && j == 0) c = 0xC9; 
                    else if (i == 0 && j == win_w - 1) c = 0xBB; 
                    else if (i == win_h - 1 && j == 0) c = 0xC8; 
                    else if (i == win_h - 1 && j == win_w - 1) c = 0xBC; 
                    else if (i == 0 || i == win_h - 1) c = 0xCD; 
                    else if (j == 0 || j == win_w - 1) c = 0xBA; 
                    
                    putchar_at(c, win_col, start_x + j, start_y + i);
                }
            }
            print_at(" AaronOS Explorer", win_col, start_x + 2, start_y);
        }
        else if (kstrcmp(input_buffer, "time") == 0) {
            read_rtc();
            char time_str[16];
            print("Clock ["); print(current_tz_name); print("]: ");
            itoa(system_time.hour, time_str, 10); print(time_str); print(":");
            if (system_time.minute < 10) print("0");
            itoa(system_time.minute, time_str, 10); print(time_str); print(":");
            if (system_time.second < 10) print("0");
            itoa(system_time.second, time_str, 10); print(time_str);
            print(" | Date: ");
            itoa(system_time.month, time_str, 10); print(time_str); print("/");
            itoa(system_time.day, time_str, 10); print(time_str); print("/");
            itoa(system_time.year, time_str, 10); print(time_str);
        }
        else if (kstrncmp(input_buffer, "tz", 2) == 0) {
            if (kstrlen(input_buffer) <= 3) {
                print("Unknown city. Defaults: amsterdam, london, newyork, tokyo");
            } 
            else {
                char* city = &input_buffer[2]; 
                
                if (kstrcmp(city, " amsterdam") == 0) {
                    current_offset = 2; 
                    kstrcpy(current_tz_name, "Amsterdam (CEST)");
                    print("Zone: Europe/Amsterdam (GMT+2)");
                } else if (kstrcmp(city, " london") == 0) {
                    current_offset = 1; 
                    kstrcpy(current_tz_name, "London (BST)");
                    print("Zone: Europe/London (GMT+1)");
                } else if (kstrcmp(city, " newyork") == 0) {
                    current_offset = -4; 
                    kstrcpy(current_tz_name, "New York (EDT)");
                    print("Zone: America/New_York (GMT-4)");
                } else if (kstrcmp(city, " tokyo") == 0) {
                    current_offset = 9; 
                    kstrcpy(current_tz_name, "Tokyo (JST)");
                    print("Zone: Asia/Tokyo (GMT+9)");
                } else {
                    print("Unknown city. Defaults: amsterdam, london, newyork, tokyo");
                }
            }
        }
        else if (kstrncmp(input_buffer, "beep ", 5) == 0) {
            int freq = katoi(&input_buffer[5]);
            if (freq > 0 && freq < 20000) {
                print("Tuning PIT to "); print(&input_buffer[5]); print(" Hz.");
                play_sound(freq); sleep(40); nosound();
            } else {
                print("Freq Out of Range (1-20000)");
            }
        }
        else if (kstrcmp(input_buffer, "beep") == 0) boot_jingle();
        else if (kstrcmp(input_buffer, "reboot") == 0) sys_reboot();
        else if (kstrcmp(input_buffer, "shutdown") == 0) {
            print_col("Powering off...", COLOR_ALERT);
            outw(0x604, 0x2000); 
        }
        else if (kstrcmp(input_buffer, "ver") == 0) {
            print_col(KERNEL_NAME, COLOR_SUCCESS); 
            print(" ["); print(KERNEL_VERSION); print("]\n");
            print("Architecture: i386 Monolithic\n");
            print("Build: "); print(KERNEL_BUILD);
        }
        else if (kstrcmp(input_buffer, "cls") == 0) clear_screen();
        else if (kstrcmp(input_buffer, "install") == 0) run_installation();
        else if (kstrcmp(input_buffer, "panic") == 0) kpanic("USER_INITIATED_TEST");
        else if (kstrcmp(input_buffer, "cpu") == 0) {
            uint32_t ebx, ecx, edx;
            asm volatile("cpuid" : "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
            char vendor[13];
            *((uint32_t*)vendor) = ebx;
            *((uint32_t*)(vendor + 4)) = edx;
            *((uint32_t*)(vendor + 8)) = ecx;
            vendor[12] = '\0';
            print("Processor: "); print_col(vendor, COLOR_HELP);
        }
        else if (kstrcmp(input_buffer, "dir") == 0) fat16_list_files();
        else if (kstrcmp(input_buffer, "ls") == 0) {
            print_col("DIRECTORY LISTING:\n", COLOR_HELP);
            fat16_list_files(); 
        }
        else if (kstrncmp(input_buffer, "cat ", 4) == 0) fat16_cat(&input_buffer[4]);
        else if (kstrncmp(input_buffer, "write ", 6) == 0) fat16_write_to_test(&input_buffer[6]);
        else if (kstrncmp(input_buffer, "touch ", 6) == 0) {
            fat16_create_file(&input_buffer[6]);
            print("File created successfully.");
        }
        else if (kstrncmp(input_buffer, "rm ", 3) == 0) {
            fat16_delete_file(&input_buffer[3]);
            print("File deleted successfully.");
        }
        else if (kstrncmp(input_buffer, "rename ", 7) == 0) {
            char* args = &input_buffer[7];
            char* space = kstrchr(args, ' ');
            if (space) {
                *space = '\0';
                char* new_name = space + 1;
                fat16_rename_file(args, new_name);
                print("Rename requested.");
            } else {
                print("Syntax: rename [old] [new]");
            }
        }
        else if (kstrncmp(input_buffer, "echo ", 5) == 0) print(&input_buffer[5]);
        else if (kstrncmp(input_buffer, "memo ", 5) == 0) {
            fat16_write_to_test(&input_buffer[5]);
            print("Data committed to block storage.");
        }
        else if (kstrcmp(input_buffer, "music") == 0) {
            print("Audio Stream: Victory Theme");
            uint32_t notes[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
            uint32_t durations[] = {10, 10, 10, 30};
            play_song(notes, durations, 4);
        }
        else if (kstrcmp(input_buffer, "siren") == 0) {
            print("Generating Waveform: Siren");
            uint32_t notes[] = {880, 440, 880, 440, 880, 440};
            uint32_t durations[] = {35, 35, 35, 35, 35, 35};
            play_song(notes, durations, 6);
        }
        else if (kstrcmp(input_buffer, "credits") == 0) show_credits();
        else if (kstrcmp(input_buffer, "stats") == 0) print_stats();
        else if (kstrcmp(input_buffer, "rand") == 0) {
            char buf[16];
            uint8_t low = inb(0x40); 
            uint8_t time_mix = get_rtc_register(0x00);
            int pseudo_rand = (low ^ time_mix) + timer_ticks;
            itoa(pseudo_rand, buf, 10);
            print("Entropy Output: "); print(buf);
        }
        else if (kstrcmp(input_buffer, "matrix") == 0) run_matrix();
        else if (kstrncmp(input_buffer, "color ", 6) == 0) {
            int new_color = katohex(&input_buffer[6]);
            if (new_color >= 0 && new_color <= 0xFF) {
                current_term_color = (uint8_t)new_color;
                print("Terminal color updated.");
            } else {
                print("Invalid color code. Use hex format (e.g. color 0A)");
            }
        }
        else if (kstrncmp(input_buffer, "calc ", 5) == 0) {
            char* expr = &input_buffer[5];
            int a = 0, b = 0, res = 0;
            char op = 0;
            
            int i = 0;
            while(expr[i] == ' ') i++;
            a = katoi(&expr[i]);
            
            while(expr[i] >= '0' && expr[i] <= '9') i++;
            while(expr[i] == ' ') i++;
            
            if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/') {
                op = expr[i];
                i++;
                while(expr[i] == ' ') i++;
                b = katoi(&expr[i]);
                
                if (op == '+') res = a + b;
                else if (op == '-') res = a - b;
                else if (op == '*') res = a * b;
                else if (op == '/') {
                    if (b != 0) res = a / b;
                    else print("ERR: Div by 0");
                }
                
                if (op != '/' || b != 0) {
                    char buf[16];
                    itoa(res, buf, 10);
                    print("Result: "); print(buf);
                }
            } else {
                print("Syntax: calc [a] [+|-|*|/] [b]");
            }
        }
        else {
            print("Unknown command. Type help for commands.");
        }
    }
    
    print("\nAaronOS> ");
    input_ptr = 0;
    execute_flag = 0;
    prompt_limit = cursor_x;
    update_cursor();
}

/* ========================================================================== */
/* 12. SEGMENTATION & DESCRIPTORS                                             */
/* ========================================================================== */

struct gdt_entry {
    uint16_t limit_low; uint16_t base_low;
    uint8_t  base_middle; uint8_t  access;
    uint8_t  granularity; uint8_t  base_high;
} __attribute__((packed)) gdt[3];

struct gdt_ptr {
    uint16_t limit; uint32_t base;
} __attribute__((packed)) gp;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void init_gdt() {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint32_t)&gdt;
    gdt_set_gate(0, 0, 0, 0, 0);                
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); 
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); 
}

struct idt_entry {
    uint16_t base_lo, sel;
    uint8_t always0, flags;
    uint16_t base_hi;
} __attribute__((packed)) idt[256];

struct idt_ptr {
    uint16_t limit; uint32_t base;
} __attribute__((packed)) idtp;

extern void load_idt(uint32_t);

void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1; 
    idtp.base = (uint32_t)&idt;
    kmemset(idt, 0, sizeof(idt));
    
    uint32_t th = (uint32_t)timer_handler_asm;
    idt[32].base_lo = th & 0xFFFF;
    idt[32].base_hi = (th >> 16) & 0xFFFF;
    idt[32].sel = 0x08; idt[32].always0 = 0; idt[32].flags = 0x8E;

    uint32_t kh = (uint32_t)keyboard_handler_asm;
    idt[33].base_lo = kh & 0xFFFF;
    idt[33].base_hi = (kh >> 16) & 0xFFFF;
    idt[33].sel = 0x08; idt[33].always0 = 0; idt[33].flags = 0x8E;
    
    load_idt((uint32_t)&idtp);
}

/* ========================================================================== */
/* 13. KERNEL ENTRY POINT                                                     */
/* ========================================================================== */

void kernel_main() {
    init_gdt();
    
    outb(0x20, 0x11); io_wait(); outb(0x21, 0x20); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0x21, 0x01); io_wait();
    outb(0xA0, 0x11); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0xA1, 0x02); io_wait(); outb(0xA1, 0x01); io_wait();
    
    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF);
    
    init_idt();
    init_timer(100); 

    sys_stats.uptime_ticks = 0;
    sys_stats.total_commands = 0;
    sys_stats.speaker_state = 0;
    
    clear_screen();
    cursor_y = 0;
    update_cursor();
    
    asm volatile("sti");
    boot_jingle();
    
    print("Welcome to AaronOS! \n Use help for commands.\n");
    print("AaronOS> ");

    while (1) { 
        if (execute_flag == 1 ) {
            process_shell(); 
            execute_flag = 0;
        }
        asm volatile("hlt"); 
    }
}

/**
 * =============================================================================
 * END OF KERNEL.C
 * =============================================================================
 */
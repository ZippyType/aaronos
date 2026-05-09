#ifndef COMMANDS_H
#define COMMANDS_H

#define CMD(name, desc) { name, desc }

typedef struct {
    const char* name;
    const char* description;
} command_t;

command_t commands[] = {
    CMD("help",     "Show all commands"),
    CMD("gui",      "Open TUI Explorer"),
    CMD("ver",      "Show system version"),
    CMD("reboot",   "Warm restart"),
    CMD("shutdown", "Power off"),
    CMD("cls",      "Clear terminal"),
    CMD("dmesg",    "Show boot log"),
    CMD("install",  "Run HDD deployment"),
    CMD("edit",     "Open text editor"),
    CMD("panic",    "Test kernel crash"),
    CMD("cpu",      "Show CPU vendor"),
    CMD("credits",  "Show build info"),
    CMD("stats",    "Show health & uptime"),
    CMD("time",     "Show clock & date"),
    CMD("tz",       "Set timezone (tz amsterdam|london|newyork|tokyo)"),
    CMD("dir",      "List disk files"),
    CMD("ls",       "List disk files"),
    CMD("cat",      "Read file (cat filename)"),
    CMD("write",    "Write to file (write text)"),
    CMD("touch",    "Create file (touch filename)"),
    CMD("rm",       "Delete file (rm filename)"),
    CMD("rename",   "Rename file (rename old new)"),
    CMD("format",   "Format disk"),
    CMD("echo",     "Print text (echo text)"),
    CMD("rand",     "Random number"),
    CMD("color",    "Set color (color hex)"),
    CMD("calc",     "Math (calc 5+5 or calc sin 90)"),
    CMD("beep",     "Play tone (beep 440)"),
    CMD("music",    "Play melody"),
    CMD("siren",    "Play siren"),
    CMD("matrix",   "Enter the matrix"),
    CMD("netstat",  "Network status"),
    CMD("web",      "Web browser (web ip)"),
    CMD("ping",     "Ping host (ping ip)"),
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(command_t))

#endif
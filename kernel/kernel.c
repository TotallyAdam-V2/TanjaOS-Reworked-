#include "../include/fs.h"
#include "../include/net.h"
#include "log.h"
#include "elib.h"
#include "panic.h"
#include <stdint.h>
#include <stddef.h>

#define VGA_COLOR (0x0F << 8)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

uint16_t* VGA = (uint16_t*)0xB8000;
int cursor = 0;

uint32_t boot_time_ms = 0;
uint32_t boot_ticks = 0;

typedef struct Command {
    char name[32];
    void (*func)(char* args);
    struct Command* next;
} Command;

Command* cmd_table = 0;
int cmd_count = 0;

#define CMD_POOL_SIZE 128

Command cmd_pool[CMD_POOL_SIZE];
int cmd_pool_index = 0;

int caps_lock = 0;

#define KEY_UP        0x80
#define KEY_DOWN      0x81
#define KEY_LEFT      0x82
#define KEY_RIGHT     0x83
#define KEY_ENTER     0x84
#define KEY_BACKSPACE 0x85

#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define MAX_HOSTNAME 64

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char hostname[MAX_HOSTNAME];
    int is_setup;
} user_config_t;

user_config_t config = { .is_setup = 0 };

int shell_exit_flag = 0;

void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init() {
    outb(0x43, 0x34);
    uint16_t divisor = 1193180 / 1000; 
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void timer_delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 1000; j++) {
            asm volatile("nop");
        }
        boot_time_ms++;
    }
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void sync_cursor() {
    int max = VGA_WIDTH * VGA_HEIGHT - 1;
    if (cursor < 0) cursor = 0;
    if (cursor > max) cursor = max;

    outb(0x3D4, 0x0A);                 
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);  
    
    outb(0x3D4, 0x0B);                 
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); 

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(cursor & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((cursor >> 8) & 0xFF));
}

void timer_tick() {
    boot_ticks++;
}

void timer_handler() {
    boot_ticks++;
    outb(0x20, 0x20); 
}

void underline_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0D);

    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
}

void scroll() {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        VGA[i] = VGA[i + VGA_WIDTH];
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA[i] = VGA_COLOR | ' ';
    cursor = VGA_WIDTH * (VGA_HEIGHT - 1);
}

void putc_color(char c, uint16_t color) {
    if (c == '\n') {
        cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            VGA[cursor] = color | ' ';
        }
    } else if (c >= ' ') {
        VGA[cursor] = color | (uint8_t)c;
        cursor++;
    }
    if (cursor >= VGA_WIDTH * VGA_HEIGHT) scroll();
    sync_cursor();
}

void putc(char c) {
    putc_color(c, VGA_COLOR);
}

void print(const char* s) {
    if (!s) return;
    while (*s) putc(*s++);
}

void print_color(const char* s, uint16_t color) {
    if (!s) return;
    while (*s) putc_color(*s++, color);
}

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA[i] = VGA_COLOR | ' ';
    cursor = 0;
    sync_cursor();
}

void print_hex(uint32_t n) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];
    buffer[0] = '0'; buffer[1] = 'x'; buffer[10] = 0;
    for (int i = 9; i >= 2; i--) { buffer[i] = hex_chars[n & 0xF]; n >>= 4; }
    print(buffer);
}

void print_dec(uint32_t n) {
    if (n == 0) { putc('0'); return; }
    char buffer[11]; int pos = 10; buffer[pos] = 0;
    while (n > 0 && pos > 0) { pos--; buffer[pos] = '0' + (n % 10); n /= 10; }
    print(&buffer[pos]);
}

void boot_log(const char* msg) {
    print("[ OK ] ");
    print(msg);
    print("\n");
}

int shift = 0;
int ctrl = 0;

char keymap[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',
    8,9,'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' '
};

char keymap_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+',
    8,9,'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' '
};

char keymap_caps[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',
    8,9,'Q','W','E','R','T','Y','U','I','O','P','[',']','\n',
    0,'A','S','D','F','G','H','J','K','L',';','\'','`',
    0,'\\','Z','X','C','V','B','N','M',',','.','/',
    0,'*',0,' '
};

int get_key() {
    while (1) {
        if (!(inb(0x64) & 1)) continue;
        uint8_t sc = inb(0x60);
        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0x1D) { ctrl = 1; continue; }
        if (sc == 0x9D) { ctrl = 0; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }
        if (sc == 0x3A) { caps_lock = !caps_lock; continue; }
        if (sc == 0xE0) {
            while (!(inb(0x64) & 1)) continue;
            uint8_t ext = inb(0x60);
            if (ext == 0x48) return KEY_UP;
            if (ext == 0x50) return KEY_DOWN;
            if (ext == 0x4B) return KEY_LEFT;
            if (ext == 0x4D) return KEY_RIGHT;
            continue;
        }
        if (sc & 0x80) continue;
        if (sc >= 128) continue;
        char res;
        if (caps_lock && !shift) res = keymap_caps[sc];
        else if (shift) res = keymap_shift[sc];
        else res = keymap[sc];
        if (res == 0) continue;
        if (ctrl) {
            if (res == 'x') return 24;
        }
        return res;
    }
}

int streq(const char* a, const char* b) {
    if (!a || !b) return a == b;
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

void clean(char* s) {
    if (!s) return;
    for (int i = 0; s[i]; i++)
        if (s[i] == '\n' || s[i] == '\r') s[i] = 0;
}

#define INPUT_BUFFER_SIZE 4096

void read_line(char* buffer, int max_len) {
    char line[INPUT_BUFFER_SIZE];
    int pos = 0;
    int len = 0;
    int prompt_start = cursor;

    while (1) {
        int key = get_key();

        if (key == '\n' || key == KEY_ENTER) {
            putc('\n');
            break;
        }

        if (key == 8 || key == KEY_BACKSPACE) {
            if (pos > 0) {
                for (int i = pos - 1; i < len - 1; i++)
                    line[i] = line[i + 1];

                len--;
                pos--;
                line[len] = 0;
                cursor = prompt_start;

                for (int i = 0; i < len; i++) {
                    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
                        scroll();
                        prompt_start -= VGA_WIDTH;
                        cursor = prompt_start + i;
                    }
                    VGA[cursor] = VGA_COLOR | line[i];
                    cursor++;
                }

                if (cursor < VGA_WIDTH * VGA_HEIGHT)
                    VGA[cursor] = VGA_COLOR | ' ';

                cursor = prompt_start + pos;
                sync_cursor();
            }
            continue;
        }

        if (key == KEY_LEFT) {
            if (pos > 0) {
                pos--;
                cursor = prompt_start + pos;
                sync_cursor();
            }
            continue;
        }

        if (key == KEY_RIGHT) {
            if (pos < len) {
                pos++;
                cursor = prompt_start + pos;
                sync_cursor();
            }
            continue;
        }

        if (key == KEY_UP || key == KEY_DOWN)
            continue;

        if (key >= 32 && key <= 126) {
            if (len < INPUT_BUFFER_SIZE - 1) {
                for (int i = len; i > pos; i--)
                    line[i] = line[i - 1];

                line[pos] = key;
                len++;
                pos++;
                line[len] = 0;
                cursor = prompt_start;

                for (int i = 0; i < len; i++) {
                    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
                        scroll();
                        prompt_start -= VGA_WIDTH;
                        cursor = prompt_start + i;
                    }
                    VGA[cursor] = VGA_COLOR | line[i];
                    cursor++;
                }

                if (prompt_start + pos >= VGA_WIDTH * VGA_HEIGHT) {
                    scroll();
                    prompt_start -= VGA_WIDTH;
                }

                cursor = prompt_start + pos;
                sync_cursor();
            }
        }
    }

    int copy_len = len;
    if (copy_len >= max_len)
        copy_len = max_len - 1;

    for (int i = 0; i < copy_len; i++)
        buffer[i] = line[i];

    buffer[copy_len] = 0;
}

void register_cmd(const char* name, void (*func)(char* args)) {
    if (cmd_pool_index >= CMD_POOL_SIZE) {
        LOG_WARN("Command pool limit reached when registering: %s", name);
        return;
    }

    Command* cmd = &cmd_pool[cmd_pool_index++];
    int i = 0;
    while (name[i] && i < 31) {
        cmd->name[i] = name[i];
        i++;
    }

    cmd->name[i] = 0;
    cmd->func = func;
    cmd->next = 0;

    if (cmd_table == 0) {
        cmd_table = cmd;
    } else {
        Command* current = cmd_table;
        while (current->next)
            current = current->next;
        current->next = cmd;
    }

    cmd_count++;

    // Ensure /bin directory exists before writing compiled commands
    fs_create_directory("/bin");
    
    char path[64] = "/bin/";
    int p_idx = 5;
    int n_idx = 0;
    while (name[n_idx] && p_idx < 48) {
        path[p_idx++] = name[n_idx++];
    }
    path[p_idx++] = '.';
    path[p_idx++] = 'b';
    path[p_idx++] = 'i';
    path[p_idx++] = 'n';
    path[p_idx] = 0;

    char file_content[128];
    int fc_idx = 0;
    const char* header = "TANJAOS_COMPILED_CMD: ";
    while (*header) file_content[fc_idx++] = *header++;
    n_idx = 0;
    while (name[n_idx] && fc_idx < 120) file_content[fc_idx++] = name[n_idx++];
    file_content[fc_idx] = 0;

    fs_write_file(path, file_content, fc_idx);
    LOG_DEBUG("Registered command and compiled bin file: %s", path);
}

int cmd_exists(const char* name) {
    if (!name || !*name) return 0;
    Command* cmd = cmd_table;
    while (cmd) {
        if (streq(cmd->name, name)) return 1;
        cmd = cmd->next;
    }
    return 0;
}

void list_commands(void) {
    print("\nAvailable commands:\n\n");
    Command* cmd = cmd_table;
    int col = 0;

    while (cmd) {
        int len = 0;
        while (cmd->name[len])
            len++;

        if (col + len + 3 >= VGA_WIDTH) {
            print("\n");
            col = 0;
        }

        print(cmd->name);
        print(" ");
        col += len + 3;
        cmd = cmd->next;
    }

    print("\n\n");
}

void execute_command(const char* cmd_line) {
    while (*cmd_line == ' ')
        cmd_line++;

    if (!*cmd_line)
        return;

    char cmd_name[32];
    int i = 0;

    while (cmd_line[i] && cmd_line[i] != ' ' && i < 31) {
        cmd_name[i] = cmd_line[i];
        i++;
    }

    cmd_name[i] = 0;
    const char* args = cmd_line + i;

    while (*args == ' ')
        args++;

    LOG_INFO("Executing command -> Name: %s | Args: %s", cmd_name, args);

    Command* cmd = cmd_table;
    while (cmd) {
        if (streq(cmd->name, cmd_name)) {
            cmd->func((char*)args);
            return;
        }
        cmd = cmd->next;
    }

    LOG_WARN("Command execution failed: '%s' not found", cmd_name);
    print("error: Command not found: ");
    print(cmd_name);
    print("\n");
}

void core_hardware_init(void) {
    LOG_INFO("Initializing base hardware systems");
    underline_cursor();
    clear_screen();
    
    print("----------------------------------------\n");
    print("       TanjaOS [v2.0 Reworked]          \n");
    print("       Core Subsystem Initialization    \n");
    print("----------------------------------------\n");

    timer_init();
    boot_log("Hardware timer configured successfully");
    LOG_INFO("PIT timer initialized at 1000Hz frequency");
    timer_delay_ms(30);
    boot_log("PIT Timer Verification in proggress");
    timer_delay_ms(4500);
    timer_handler();
    timer_delay_ms(40);
    boot_log("PIT Timer Verified Continuing Booting proggress.");
    timer_delay_ms(10);
}

void core_subsystems_init(void) {
    LOG_INFO("Mounting core operating system subsystems");
    
    boot_log("Initializing filesystem module");
    fs_init();
    LOG_INFO("Filesystem mounted and verified");
    timer_delay_ms(30);

    boot_log("Creating system core directories (/home, /bin)");

    int home_res = fs_create_directory("/home");
    int bin_res = fs_create_directory("/bin");
    
    if (home_res < 0) {
        LOG_WARN("Failed to create /home directory automatically (code: %d)", home_res);
    } else {
        LOG_INFO("/home directory created successfully");
    }

    if (bin_res < 0) {
        LOG_WARN("Failed to create /bin directory automatically (code: %d)", bin_res);
    } else {
        LOG_INFO("/bin directory created successfully");
    }

    timer_delay_ms(30);

    boot_log("Initializing Network Stack");
    net_init();
    LOG_INFO("Network subsystem initialized");
    timer_delay_ms(30);

    boot_log("Initializing Serial Logging System");
    log_init();
    LOG_INFO("Serial communication interface online");
    timer_delay_ms(30);
}
void setup_wizard() {
    LOG_INFO("Launching user setup wizard interface");
    print("========================================\n");
    print("       TanjaOS Initial Setup Wizard     \n");
    print("========================================\n\n");
    
    print(" [Account] Username : "); read_line(config.username, MAX_USERNAME);
    print(" [Security] Password: "); read_line(config.password, MAX_PASSWORD);
    print(" [Network] Hostname : "); read_line(config.hostname, MAX_HOSTNAME);
    
    net_set_hostname(config.hostname);
    config.is_setup = 1;
    LOG_INFO("Setup completed successfully for user: %s at host: %s", config.username, config.hostname);
    clear_screen();
}

void login_prompt() {
    int failed_attempts = 0;
    char u[MAX_USERNAME], p[MAX_PASSWORD];
    while (1) {
        print("----------------------------------------\n");
        print("            System Login                \n");
        print("  If login fails 4 times OS will Panic  \n");
        print("----------------------------------------\n");
        print("["); print(config.hostname); print("]"); print(" login: "); read_line(u, MAX_USERNAME);
        print("Password: "); read_line(p, MAX_PASSWORD);
        
        if (streq(u, config.username) && streq(p, config.password)) { 
            LOG_INFO("User '%s' authenticated successfully", u);
            print("\n"); 
            return; 
        }

        failed_attempts++;
        if (failed_attempts >= 4) {
            clear_screen();
            PANIC("System Authentication Failed 4 times.");
        }

        LOG_WARN("Failed authentication attempt for username: %s (Attempt %d/4)", u, failed_attempts);
        print("Authentication failed. Try again.\n\n");
    }
}

void cmd_exit(char* args) { 
    (void)args; 
    LOG_INFO("Shell exit sequence invoked");
    shell_exit_flag = 1; 
}

void cmd_hostname(char* args) {
    if (args && args[0]) {
        int i = 0;
        while (i < MAX_HOSTNAME-1 && args[i] && args[i] != ' ') { 
            config.hostname[i] = args[i]; 
            i++; 
        }
        config.hostname[i] = 0; 
        net_set_hostname(config.hostname);
        LOG_INFO("Hostname modified dynamically to: %s", config.hostname);
        print("Hostname updated\n");
    } else { 
        print(net_get_hostname()); 
        print("\n"); 
    }
}

void print_prompt_path() {
    char cwd[256];
    fs_get_current_path(cwd);
    if (cwd[0] == '/' && cwd[1] == 0) {
        print("~");
    } else {
        print("~");
        print(cwd);
    }
}

void shell() {
    shell_exit_flag = 0; 
    
    int cd_res = fs_change_directory("/home");
    if (cd_res < 0) {
        fs_create_directory("home");
        cd_res = fs_change_directory("home");
    }

    if (cd_res < 0) {
        LOG_WARN("Shell failed to change directory to /home (error code: %d). Staying in root.", cd_res);
        print("Warning: Could not switch to /home directory.\n");
    } else {
        LOG_INFO("Shell starting directory successfully set to /home");
    }
    
    char buf[4096];
    while (1) {
        print(config.username);
        print("@");
        print(config.hostname);
        print(":");
        print_prompt_path();
        print("$ ");
        read_line(buf, 4096); 
        clean(buf);
        if (buf[0]) execute_command(buf);
        if (shell_exit_flag) { clear_screen(); break; }
    }
}
extern void init_cmds(void);

void kernel_main() {  
    core_hardware_init();
    core_subsystems_init();

    LOG_INFO("Registering internal command structures");
    boot_log("Loading command extensions");
    init_cmds();
    timer_delay_ms(30);

    boot_log("Validating command table metrics");
    LOG_DEBUG("Total commands discovered: %d", cmd_count);

    if (cmd_count == 0) {
        LOG_FATAL("Critical kernel panic: Command registry is empty (cmd_count == 0)");
        print("panic: due to: Unable to load commands\n");
        print("command count=0\n");
        print("Please provide commands in cmd\n");
        print("panic: due to: Unable to load commands\n");
        print("System Now has halted using Emergacy Halt\n");

        PANIC("Unable to load system commands! Registry verification failed.");

        while (1);
    }

    register_cmd("exit", cmd_exit);
    register_cmd("hostname", cmd_hostname);
    timer_delay_ms(30);

    boot_log("Preparing initialization state");
    LOG_INFO("Transitioning to interactive configuration wizard");

    if (!config.is_setup)
        print("\n");
    setup_wizard();

    timer_delay_ms(30);
    boot_log("Launching interactive shell environment");
    print("\n");

    while (1) {
        LOG_INFO("Shell Session Context Active");
        print("========================================\n");
        print("    TanjaOS Active Interactive Shell    \n");
        print("========================================\n\n");

        login_prompt();
        shell();
    }
}
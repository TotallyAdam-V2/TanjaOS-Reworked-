#include "cmd.h"
#include "../kernel/elib.h"

void cmd_halt(char *args) {
    extern void clear_screen(void);
    extern void print(const char* s);
    extern void cpu_hlt(int code);

    clear_screen();
    print("TanjaOS Is now halting.\n\n");
    cpu_hlt(3113);
}
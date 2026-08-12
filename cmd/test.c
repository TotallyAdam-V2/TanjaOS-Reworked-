#include "cmd.h"

void cmd_test(char* args ) {
    extern void print(const char* s);

    print("Command Test\n");
}
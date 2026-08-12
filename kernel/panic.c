/*
TanjaOS [Reworked] Project
File: panic.c
Created: TotallyAdam-V2
Modified By: TotallyAdam-V2
*/

#include "panic.h"
#include "log.h"

extern void print(const char* s);
extern void hlt_emergency(const char* message);

void kernel_panic(const char* file, int line, const char* reason) {
    LOG_FATAL("KERNEL PANIC at %s:%d - %s", file, line, reason);

    print("\n==================== KERNEL PANIC ====================\n");
    print("A fatal unrecoverable error occurred in the kernel.\n");
    print("Reason: ");
    print(reason);
    print("\nSystem has halted safely via Emergency Halt.\n");
    print("======================================================\n");

    hlt_emergency(reason);
}
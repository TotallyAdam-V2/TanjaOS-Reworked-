#include <stddef.h>
#include "elib.h"
#include "log.h"

void cpu_hlt(void) {
    // Disable interrupts and halt the CPU indefinitely
    __asm__ volatile (
        "cli\n\t"     // Clear interrupt flag (disable interrupts)
        "1:\n\t"     // Local label 1
        "hlt\n\t"    // Halt the CPU
        "jmp 1b\n\t" // Jump back to label 1 just in case an NMI wakes it
    );
}

void hlt_emergency(const char* message) {
    // 1. Disable maskable interrupts immediately
    __asm__ volatile ("cli");

    // 2. Log the fatal error if the logging system is available
    if (message != NULL) {
        LOG_FATAL("EMERGENCY HALT: %s", message);
    } else {
        LOG_FATAL("EMERGENCY HALT: Unknown fatal error occurred.");
    }

    // 3. Enter an infinite, low-power halted state
    while (1) {
        __asm__ volatile ("hlt");
    }
}
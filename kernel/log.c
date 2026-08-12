/*
TanjaOS [Reworked] Project
File: log.c
Created: TotallyAdam-V2
Modified By: TotallyAdam-V2 (Note for contrybutors when you modifie this file put your github name here)
*/

#include "log.h"
#include <stdarg.h>

// Standard COM1 I/O port base address
#define PORT 0x3F8

// Inline assembly helper functions for x86 port I/O
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Check if the transmit holding register is empty
static int is_transmit_empty(void) {
    return inb(PORT + 5) & 0x20;
}

// Write a single character to the serial port
static void serial_putc(char c) {
    while (is_transmit_empty() == 0);
    outb(PORT, c);
}

// Write a null-terminated string to the serial port
static void serial_puts(const char* str) {
    while (*str) {
        serial_putc(*str++);
    }
}

void log_init(void) {
    outb(PORT + 1, 0x00);    // Disable interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38200 baud
    outb(PORT + 1, 0x00);    //             (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

// Simple internal function to convert an integer to a string
static void int_to_str(long val, char* buf, int base) {
    char* p = buf;
    char* p1, *p2;
    unsigned long uval = (val < 0 && base == 10) ? -val : val;

    do {
        int remainder = uval % base;
        *p++ = (remainder < 10) ? (remainder + '0') : (remainder - 10 + 'A');
    } while ((uval /= base) > 0);

    if (val < 0 && base == 10) *p++ = '-';
    *p = '\0';

    // Reverse the string
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
}

// Basic custom vprintf implementation for kernel logging
void log_write(log_level_t level, const char* format, ...) {
    // Prefix strings for severity levels
    const char* level_strings[] = {
        "[DEBUG] ",
        "[INFO]  ",
        "[WARN]  ",
        "[ERROR] ",
        "[FATAL] "
    };

    if (level >= 0 && level <= 5) {
        serial_puts(level_strings[level]);
    }

    va_list args;
    va_start(args, format);

    const char* traverse = format;
    while (*traverse != '\0') {
        if (*traverse == '%') {
            traverse++;
            switch (*traverse) {
                case 's': {
                    char* s = va_arg(args, char*);
                    serial_puts(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    long i = va_arg(args, int);
                    char buf[32];
                    int_to_str(i, buf, 10);
                    serial_puts(buf);
                    break;
                }
                case 'x': {
                    unsigned int i = va_arg(args, unsigned int);
                    char buf[32];
                    int_to_str(i, buf, 16);
                    serial_puts("0x");
                    serial_puts(buf);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    serial_putc(c);
                    break;
                }
                default:
                    serial_putc('%');
                    serial_putc(*traverse);
                    break;
            }
        } else {
            serial_putc(*traverse);
        }
        traverse++;
    }

    va_end(args);
    serial_putc('\n');
}
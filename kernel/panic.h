/*
TanjaOS [Reworked] Project
File: panic.h
Created: TotallyAdam-V2
Modified By: TotallyAdam-V2
*/

#ifndef PANIC_H
#define PANIC_H

void kernel_panic(const char* file, int line, const char* reason);

#define PANIC(reason) kernel_panic(__FILE__, __LINE__, reason)

#endif
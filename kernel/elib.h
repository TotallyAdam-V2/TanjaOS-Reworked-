/*
eLib Project
Created by: TotallyAdam-V2
Currently Used for: TanjaOS [Reworked]
*/

#ifndef ELIB_H
#define ELIB_H

#define AUTHCODE 3113  // Define it as a clean macro constant

void cpu_hlt(int code);
void hlt_emergency(const char* message);

#endif // ELIB_H
#ifndef GAME_H
#define GAME_H

/*
TanjaOS [Reworked] Project
File: game.h
Created: MSK-Kernel
Modified By: MSK-Kerenle (Note for contrybutors when you modifie this file put your github name here)
*/

void delay(unsigned int ticks);

int key_available(void);
int get_key(void);

void clear_screen(void);
void putc(char c);
void print(const char *s);

#endif

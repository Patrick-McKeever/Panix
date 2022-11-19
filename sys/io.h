#ifndef IO_H
#define IO_H

#include <stdint.h>


// IO port read/write instructions for bytes, words, and dwords.

uint8_t inportb(uint16_t port);
void outportb(uint16_t port, uint8_t byte);

uint16_t inportw(uint16_t port);
void outportw(uint16_t port, uint16_t word);

uint32_t inportl(uint16_t port);
void outportl(uint16_t port, uint16_t dword);


#endif
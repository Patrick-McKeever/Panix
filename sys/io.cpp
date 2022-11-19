#include "io.h"


uint8_t inportb(uint16_t port)
{
    uint8_t val;
    __asm__ __volatile__("inb %0, %1"
    : "=a"(val)
    : "dN"(port));
    return val;
}

void outportb(uint16_t port, uint8_t byte)
{
    __asm__ __volatile__("outb %0, %1"
    : /* No outputs */
    : "dN"(port)
    , "a"(byte));
}

uint16_t inportw(uint16_t port)
{
    uint16_t val;
    __asm__ __volatile__("in ax, %1"
    : "=a"(val)
    : "dN"(port));
    return val;
}

void outportw(uint16_t port, uint16_t word)
{
    __asm__ __volatile__("out %0, ax"
    : /* No outputs */
    : "dN"(port)
    , "a"(word));
}

uint32_t inportl(uint16_t port)
{
    uint32_t val;
    __asm__ __volatile__("in eax, %1"
    : "=a"(val)
    : "dN"(port));
    return val;
}

void outportl(uint16_t port, uint32_t dword)
{
    __asm__ __volatile__("out %0, eax"
    : /* No outputs */
    : "dN"(port)
    , "a"(dword));
}

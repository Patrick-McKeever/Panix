#include "sys/log.h"

void LogChar(char c)
{
    outportb(COM1_PORT, c);
    outportb(0xE9, c);
}

char *Convert(char buffer[64], uint64_t num, uint8_t base)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char *ptr = &buffer[63];
    *ptr = '\0';

    do {
        *(--ptr) = hex_digits[num % base];
        num /= base;
    } while(num != 0);

    return ptr;
}

void Log(const char *string)
{
    const char *c;
    for(c = string; *c != '\0'; ++c) {
        LogChar(*c);
    }
}

void Log(const ds::String &string)
{
    for(int i = 0; i < string.Len() && string[i]; ++i) {
        LogChar(string[i]);
    }
}
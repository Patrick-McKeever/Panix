#include "libc/string.h"
#include "sys/log.h"
#include <stddef.h>
#include <stdint.h>


void *memcpy(void *dest, const void *source, size_t bytes)
{
    char *dest_bytes = (char*) dest;
    const char *source_bytes = (char*) source;
    for(size_t i = 0; i < bytes; ++i) {
        dest_bytes[i] = source_bytes[i];
    }
    return dest;
}

void *memmove(void *dest, const void *source, size_t bytes)
{
    unsigned char *dest_bytes = (unsigned char*) dest;
    const unsigned char *source_bytes = (unsigned char*) source;
    for(int i = bytes - 1; i >= 0; --i) {
        dest_bytes[i] = source_bytes[i];
    }
    return dest;
}

void  *memset(void *b, int c, size_t len)
{
    unsigned char *p = (unsigned char*) b;
    //for(unsigned char *p = (unsigned char *) b; len > 0; ++p, --len) {
    //    *p = c;
    //}
    while(len > 0)
    {
        *p = c;
        p++;
        len--;
    }
    return(b);
}

//void *memset(void *dest, int val, size_t bytes)
//{
//    int *dest_dwords = (int*) dest;
//    for(size_t i = 0; i < bytes; i += sizeof(int)) {
//        dest_dwords[i] = val;
//    }
//    return dest;
//}

char *strcat (char *dest, const char *source)
{
    size_t concat_offset = strlen(dest);
    for(size_t i = 0; i < strlen(source); ++i) {
        dest[concat_offset + i] = source[i];
    }
    dest[concat_offset + strlen(source)] = '\0';
    return dest;
}

char *strncat (char *dest, const char *source, size_t len)
{
   size_t concat_offset = strlen(dest);
   for(size_t i = 0; i < len; ++i) {
       dest[concat_offset + i] = source[i];
   }
   dest[concat_offset + len] = '\0';
   return dest;
}

int strcmp (const char *lhs, const char *rhs)
{
    for(; *lhs && *rhs && *lhs == *rhs; ++lhs, ++rhs);
    return *((const unsigned char*) lhs) -  *((const unsigned char*) rhs);
}

int strncmp (const char *lhs, const char *rhs, size_t len)
{
    size_t i;
    for(i = 0; i < len - 1 && *lhs && *rhs && *lhs == *rhs; ++lhs, ++rhs, ++i);
    return *((const unsigned char*) lhs) -  *((const unsigned char*) rhs);
}

size_t strlen(const char *str)
{
    size_t i;
    for(i = 0; *str; ++i, ++str);
    return i;
}

int memcmp (const void *str1, const void *str2, size_t count)
{
    const auto *s1 = (const unsigned char*)str1;
    const auto *s2 = (const unsigned char*)str2;

    while (count-- > 0)
    {
        if (*s1++ != *s2++)
            return s1[-1] < s2[-1] ? -1 : 1;
    }
    return 0;
}
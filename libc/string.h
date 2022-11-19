#ifndef STRING_H
#define STRING_H

#include <stddef.h>

extern "C" void *memcpy (void *dest, const void *src, size_t bytes);
extern "C" void *memmove (void *dest, const void *source, size_t bytes);
extern "C" void *memset (void *dest, int val, size_t bytes);
extern "C" int memcmp (const void *, const void *, size_t);
//void *memchr (const void *, int, size_t);
//
//char *strcpy (char *__restrict, const char *__restrict);
//char *strncpy (char *__restrict, const char *__restrict, size_t);
//
extern "C" char *strcat (char *, const char *);
extern "C" char *strncat (char *, const char *, size_t);
//
extern "C" int strcmp (const char *, const char *);
extern "C" int strncmp (const char *, const char *, size_t);
extern "C" size_t strlen(const char*);
//
//int strcoll (const char *, const char *);
//size_t strxfrm (char *__restrict, const char *__restrict, size_t);
//
//char *strchr (const char *, int);
//char *strrchr (const char *, int);
//
//size_t strcspn (const char *, const char *);
//size_t strspn (const char *, const char *);
//char *strpbrk (const char *, const char *);
//char *strstr (const char *, const char *);
//char *strtok (char *__restrict, const char *__restrict);

#endif

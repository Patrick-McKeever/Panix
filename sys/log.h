#ifndef LOG_H
#define LOG_H

#include "sys/io.h"
#include <ds/string.h>
#include <cstdint>
#include <type_traits>


namespace ds {
    template <typename T>class BaseString;
    using String = BaseString<KernelAllocator>;
}
const uint16_t COM1_PORT    =   0x3F8;

/**
 * Print a single character into QEMU log via COM1 port.
 * @param c Character to print.
 */
void LogChar(char c);

/**
 * Convert an integer to the given base, store the output in given buffer.
 * @param buffer The buffer of characters in which the result should be stored.
 *               We choose to use a buffer rather than dynamic memory allocation,
 *               because this function can be called at any time in the boot
 *               process - even before the kernel has created a heap. As such,
 *               it is preferable to use something like a stack-allocated buffer,
 *               since the max-width of the output is known and reasonably small.
 *               (Max is 64 chars for a 64-bit binary representation.)
 * @param num The number to convert.
 * @param base The base to which it should be converted.
 * @return A pointer to the buffer entry corresponding to the greatest non-zero
 *         digit.
 */
char *Convert(char buffer[64], uint64_t num, uint8_t base);

/**
 * Output a non-formatted string to the QEMU log via the COM1 port.
 * @param string The string to print.
 */
void Log(const char *string);

/**
 * Output a non-formatted string to the QEMU log via the COM1 port.
 * @param string The string to print, as a ds::string.
 */
void Log(const ds::String &string);

/**
 * Similar to printf, but with a few differences; in particular, the valid
 * format specifiers differ from printf:
 *      - "%c" denotes a character.
 *      - "%d" denotes decimal representation of a number (of any width).
 *      - "%b" denotes binary representation of a number (of any width).
 *      - "%o" denotes octal representation of a number (of any width).
 *      - "%x" denotes hex represenatation of a number (of any width).
 *      - "%s" denotes a null-terminated string (i.e. const char*).
 * Note that integer widths are not relevant here; template specialization takes
 * care of this for us.
 * @tparam first_t The type of the first argument (i.e. value that will take
 *                 place of format specifier in output string).
 * @tparam other_ts The types of the subsequent arguments.
 * @param format The format string, containing a number of the
 *               aforementioned format specifiers equivalent to the number of
 *               variadic arguments.
 * @param first_arg The value to replace the first format specifier.
 * @param other_args The values to replace subsequent format specifiers.
 */
template <typename first_t, typename... other_ts>
void Log(const char *format, const first_t &first_arg,
         const other_ts&... other_args)
{
    if(! format || *format == '\0') {
        return;
    }

    // Print chars until encountering format specifier...
    const char *c;
    for(c = format; *c != '\0' && *c != '%'; ++c) {
        LogChar(*c);
    }

    // Then parse the format specifier, and replace it with the appropriate
    // arg...
    if(*c == '%') {
        const char data_type = *(++c);
        char formatted_num[64] = {0};
        switch(data_type) {
            case 'c': {
                if constexpr(std::is_same<first_t, char>::value) {
                    LogChar(first_arg);
                }
            } break;

            case 'd':
            case 'b':
            case 'o':
            case 'x': {
                if constexpr(std::is_integral<first_t>::value) {
                    uint8_t base;
                    if (data_type == 'd') {
                        base = 10;
                    } else if (data_type == 'b') {
                        base = 2;
                    } else if (data_type == 'o') {
                        base = 8;
                    } else {
                        base = 16;
                    }

                    if constexpr(std::is_signed<first_t>::value) {
                        int64_t num = (int64_t) first_arg;
                        char*   str_start = Convert(formatted_num, num, base);
                        if (num < 0) {
                            LogChar('-');
                        }
                        Log(str_start);
                    } else {
                        uint64_t num = (uint64_t) first_arg;
                        char*   str_start = Convert(formatted_num, num, base);
                        Log(str_start);
                    }
                }
            } break;

            case 's': {
                constexpr bool is_c_str = std::is_same<first_t, char*>::value;
                constexpr bool is_str = std::is_same<first_t, ds::String>::value;
                if constexpr(is_c_str || is_str) {
                    Log(first_arg);
                }
            } break;

            default: {
                return;
            }
        }
    }

    // Then, if there's anything left in the format string, call log
    // on the remainder of the string with the remainder of the values
    // given to this function.
    if(*c != '\0') {
        // Increment before recursing in order to get rid of data type.
        Log(++c, other_args...);
    }
}

#endif
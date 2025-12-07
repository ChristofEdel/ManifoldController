#include "Stringprintf.h"

#include <stdarg.h>
#include <stdio.h>

String StringPrintf(const char* format, ...)
{
    // Sensibly sized buffer on the stack which hopefully is enough
    char localBuffer[200];
    char* buffer = localBuffer;

    // We try to pint on to the string buffer we have
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(buffer, sizeof(localBuffer), format, copy);
    va_end(copy);

    // Error --> return empty string
    if (len < 0) {
        va_end(arg);
        return emptyString;
    }

    // If we need more than the loval budder size,
    // we allocate a bigger buffer and try again
    if (len >= (int)sizeof(localBuffer)) {
        buffer = (char*)malloc(len + 1);
        if (buffer == NULL) {
            va_end(arg);
            return emptyString;
        }
        len = vsnprintf(buffer, len + 1, format, arg);
    }
    va_end(arg);

    // We now create the result string and release the buffer
    String result(buffer);
    if (buffer != localBuffer) {
        free(buffer);
    }

    return result;
}

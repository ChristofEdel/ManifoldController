#include "StringTools.h"

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

StringPrinter::StringPrinter(String& out)
    : m_out(out),
      m_reserved(out.length())
{
    if (m_reserved % 100) {
        m_reserved = ((m_reserved / 100) + 1) * 100;
        m_out.reserve(m_reserved);
    }
}

size_t StringPrinter::write(uint8_t c)
{
    expandCapacity(1);
    m_out += char(c);
    return 1;
}

size_t StringPrinter::write(const uint8_t* buffer, size_t size)
{
    expandCapacity(size);
    m_out.concat((const char*)buffer, size);
    return size;
}

void StringPrinter::expandCapacity(size_t needed)
{
    size_t len = m_out.length();
    size_t freeSpace = (m_reserved > len) ? (m_reserved - len) : 0;

    if (freeSpace >= needed) return;

    // grow m_reserved in 100-byte increments until enough
    size_t required = len + needed;
    while (m_reserved < required) {
        m_reserved += 100;
        // safer than faffing about woh modulos..
    }

    m_out.reserve(m_reserved);
}

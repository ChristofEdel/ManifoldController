#ifndef __STRING_TOOLS_H
#define __STRING_TOOLS_H

#include <Arduino.h>

String StringPrintf(const char* format, ...);
#define BOOL_TO_STRING(x) ((x) ? "true" : "false")

class StringPrinter : public Print {
  private:
    String& m_out;
    size_t m_reserved;

  public:
    explicit StringPrinter(String& out);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;

  private:
    void expandCapacity(size_t needed);
};

#endif
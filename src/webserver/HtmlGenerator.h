#ifndef __HTML_GENERATOR_H
#define __HTML_GENERATOR_H

#include <Arduino.h>

class HtmlGenerator {

  private:
    Print *r;

  public:
    HtmlGenerator (Print *printable) : r(printable) {};
    Print * getResponse() { return r; }

    void text(const char *s);
    void print(const char *s);

    void element(const char *elementName, const char *parameters, std::function<void()> func);
    void element(const char *elementName, const char *parameters, const char *contents);
    void element(const char *elementName, std::function<void()> func) { element(elementName, nullptr, func); };
    void input (const char *parameters, const char *value);
    void select (const char *parameters, std::function<void()> func) { element("select", parameters, func); };
    void option (const char*value, const char *text, bool selected);
    void blockLayout(std::function<void()> func) { element("div", "class='block-layout'", func); };
    void block(const char *title, std::function<void()> func);

    void fieldTable(std::function<void()> func) { element("table", "class='field-table'", func); };
    void fieldTableRow(const char *label, const char *parameters, std::function<void()> func);
    void fieldTableRow(const char *label, std::function<void()> func) { fieldTableRow(label, nullptr, func); };
    void fieldTableRow(const char *label, const char *text) { fieldTableRow(label, [this, text]{print("<td>"); print(text); print("</td>");}); };

    void fieldTableInput(const char *parameters, const char *value);
    void fieldTableInput(const char *parameters, int value) { fieldTableInput(parameters, String(value).c_str()); };
    void fieldTableInput(const char *parameters, double value, int precision) { fieldTableInput(parameters, String(value,precision).c_str()); };
    void fieldTableSelect(const char *parameters, std::function<void()> func);

    void fieldTableInput(const char *tdParameters, const char *parameters, const char *value);
    void fieldTableInput(const char *tdParameters, const char *parameters, int value) { fieldTableInput(parameters, String(value).c_str()); };
    void fieldTableInput(const char *tdParameters, const char *parameters, double value, int precision) { fieldTableInput(parameters, String(value,precision).c_str()); };
    void fieldTableSelect(const char *tdParameters, const char *parameters, std::function<void()> func);

    bool needsEscapingSingleQuotes(const char *s);
    String escapeSingleQuotes(const char *s);
    #define ESCAPE_SINGLE_QUOTES(s) (needsEscapingSingleQuotes(s) ? escapeSingleQuotes(s).c_str() : s)
};

#endif

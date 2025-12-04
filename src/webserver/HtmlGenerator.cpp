#include "HtmlGenerator.h"
#include "commit.h"
#include "version.h"

void HtmlGenerator::text(const char *s){
    r->print(s);    // TODO: Should escape special characters and < here
}
void HtmlGenerator::print(const char *s){
    r->print(s);
}

void HtmlGenerator::element(const char *elementName, const char *parameters, std::function<void()> func)
{
    r->print('<');
    r->print(elementName);
    if (parameters && parameters[0]) {
        r->print(' ');
        r->print(parameters);
    }
    r->println(">");
    if (func)func();
    r->print("</");
    r->print(elementName);
    r->println(">");
}

void HtmlGenerator::element(const char *elementName, const char *parameters, const char *contents)
{
    r->print('<');
    r->print(elementName);
    if (parameters && parameters[0]) {
        r->print(' ');
        r->print(parameters);
    }
    r->println(">");
    r->println(contents);
    r->print("</");
    r->print(elementName);
    r->println(">");
}

void HtmlGenerator::block(const char *title, std::function<void()> func) { 
    r->println("<div class='block'>");
        element("div", "class='title'", title); 
        element("div", "class='content'", func);
    r->println("</div>");
}

void HtmlGenerator::block(const char *title, const char *contentParameters, std::function<void()> func) { 
    String combinedParameters = contentParameters ? contentParameters : "";
    combinedParameters += " class='content'";
    r->println("<div class='block'>");
        element("div", "class='title'", title); 
        element("div", combinedParameters.c_str(), func);
    r->println("</div>");
}

void HtmlGenerator::input (const char *parameters, const char *value) {
    r->print("<input ");
    if (parameters && parameters[0]) {
        r->print(parameters);
        r->print(' ');
    }
    if (value) {
        r->print("value = '");
        r->print(ESCAPE_SINGLE_QUOTES(value));
        r->print("'");
    }
    r->println(">");
}

void HtmlGenerator::fieldTableRow(const char *label, const char *parameters, std::function<void()> func) {
    this->element("tr", [this, label, parameters, func]{
        this->element("th", parameters, [this, label]{
            this->print(label);
        });
        if (func) func();
    });
}

void HtmlGenerator::fieldTableInput(const char *parameters, const char *value) {
    r->print("<td>");
    this->input(parameters, value);
    r->println("</td>");
}

void HtmlGenerator::fieldTableInput(const char *tdParameters, const char *parameters, const char *value) {
    r->print("<td ");
    r->print(tdParameters);
    r->print(">");
    this->input(parameters, value);
    r->println("</td>");
}

void HtmlGenerator::fieldTableSelect(const char *parameters, std::function<void()> func) {
    r->print("<td>");
    this->select(parameters, func);
    r->println("</td>");
}

void HtmlGenerator::fieldTableSelect(const char *tdParameters, const char *parameters, std::function<void()> func) {
    r->print("<td ");
    r->print(tdParameters);
    r->print(">");
    this->select(parameters, func);
    r->println("</td>");
}


void HtmlGenerator::navbar(NavbarPage activePage) {
    r->print("<div class='navbar'>");
    r->print("<div class='navbox"); if (activePage == NavbarPage::Monitor) r->print(" selected"); r->print("'><a href='/monitor'>Monitor</a></div>");
    r->print("<div class='navbox"); if (activePage == NavbarPage::Files) r->print(" selected"); r->print("'><a href='/files'>Files</a></div>");
    r->print("<div class='navbox"); if (activePage == NavbarPage::Config) r->print(" selected"); r->print("'><a href='/config'>Config</a></div>");
    r->print("<div class='navbox"); if (activePage == NavbarPage::System) r->print(" selected"); r->print("'><a href='/config-system'>System</a></div>");    
    r->print("<div class='version' title='"); r->print(COMMIT); r->print("'>"); r->print(VERSION); r->print("</div>");    
    r->print("</div>");
}

bool HtmlGenerator::needsEscapingSingleQuotes(const char *cp) {
    while (*cp) {
        if (*cp == '\\' || *cp == '\'') return true;
        cp++;
    }
    return false;
}

String HtmlGenerator::escapeSingleQuotes(const char *cp) {
    String result;
    result.reserve(strlen(cp)+10);
    while (*cp) {
        if (*cp == '\\' || *cp == '\'') result += '\\'; 
        result += *cp;
        cp++;
    }
    return result;
}
void HtmlGenerator::option (const char *value, const char *text, bool selected) {
    r->print("<option");
    if (value) {
        r->print(" value='");
        r->print(ESCAPE_SINGLE_QUOTES(value));
        r->print("'");
    }
    if (selected) {
        r->print(" selected");
    }
    r->print(">");
    r->print(text);
    r->println("</option>");
}
void option (const char*value, bool selected);



#pragma once
#include <pgmspace.h>

const char STYLES_CSS_STRING[] PROGMEM = R"RAW_STRING(
table { margin: 0 auto; font-size: 12pt; border-collapse: collapse; }
table th { background: rgba(0,0,0,.1); }
table tbody th, table thead th:first-child { text-align: left; }
table th, table td { padding: .5em; border: 1px solid lightgrey; }
.delete { width: 21px; cursor: pointer; text-align: center; color: red; }
.handle { cursor: move; }
.sortable-placeholder { background: #f5f5f5; visibility: visible !important; }
)RAW_STRING";
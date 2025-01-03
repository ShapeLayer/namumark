#include <stdbool.h>
#include "inlines.h"

static inline bool S_is_line_end_char(unsigned char c) {
  return c == '\n' || c == '\r';
}

bool is_line_end_char(unsigned char c) {
  return S_is_line_end_char(c);
}

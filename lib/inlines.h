#ifndef __INLINES_H__
#define __INLINES_H__
#include <stdbool.h>

#include "node.h"
#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

bool is_line_end_char(unsigned char c);
void parse_inlines(const strbuf *source, namumark_node *parent, int line_number);

#ifdef __cplusplus
}
#endif

#endif // __INLINES_H__

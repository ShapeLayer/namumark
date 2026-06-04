/**
 * @file inlines.h
 * @brief Inline parser for links, emphasis, macros, and advanced triple braces.
 */
#ifndef __INLINES_H__
#define __INLINES_H__
#include <stdbool.h>

#include "node.h"
#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @return true when c is CR or LF. */
bool is_line_end_char(unsigned char c);
/**
 * @brief Parse inline syntax from source and append child nodes to parent.
 *
 * Unknown bracket macros are preserved as literal text at render time so that
 * UI examples like [v] do not turn into fake macro spans.
 */
void parse_inlines(const strbuf *source, namumark_node *parent, int line_number);

#ifdef __cplusplus
}
#endif

#endif // __INLINES_H__

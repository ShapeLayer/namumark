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
 *
 * @param base_column 1-based absolute byte column of source[0] within its
 *   physical line.  Child node spans are emitted as absolute line columns
 *   (base_column + byte offset) so callers can recover positions even when
 *   source is a sliced fragment (heading title, blockquote body, emphasis body).
 *   Columns are byte offsets and follow a half-open convention: end_column is
 *   one past the node's last byte, so width == end_column - start_column.
 */
void parse_inlines(const strbuf *source, namumark_node *parent, int line_number, int base_column);

#ifdef __cplusplus
}
#endif

#endif // __INLINES_H__

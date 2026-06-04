/*
buffer.h(strbuf.h), buffer.c(strbuf.c), chunk.h

are derived from code (C) 2012 Github, Inc.
(derived from "https://github.com/commonmark/cmark")

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef __STRBUF_H__
#define __STRBUF_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Signed buffer size used throughout parser code; negative values signal "not found". */
typedef int64_t bufsize_t;
#define BUFSIZE_MAX INT64_MAX

extern unsigned char strbuf__init_buf[1];

/**
 * @brief Growable byte buffer used instead of NUL-terminated strings internally.
 *
 * strbuf stores arbitrary bytes and keeps a convenience trailing NUL for C APIs.
 * Parser code should use size for bounds checks and never rely on strlen(ptr).
 */
typedef struct strbuf {
  unsigned char *ptr;
  bufsize_t asize, size;
} strbuf;

/** @brief Initialize a buffer and optionally reserve initial capacity. */
void strbuf_init(strbuf *buf, bufsize_t init_size);

/** @brief Ensure at least grow_size bytes of writable capacity. */
void strbuf_grow(strbuf *buf, bufsize_t grow_size);

/** @return The number of meaningful bytes currently stored. */
bufsize_t strbuf_len(const strbuf *buf);

/** @brief Release owned storage and reset the buffer to the shared empty state. */
void strbuf_free(strbuf *buf);
/** @brief Clear content while keeping allocated capacity. */
void strbuf_clear(strbuf *buf);

/** @brief Replace content with an explicit byte range. */
void strbuf_set(strbuf *buf, const unsigned char *str, bufsize_t len);
/** @brief Replace content with a NUL-terminated C string. */
void strbuf_sets(strbuf *buf, const char *str);

/** @brief Append one byte. */
void strbuf_putc(strbuf *buf, unsigned char c);
/** @brief Append an explicit byte range. */
void strbuf_put(strbuf *buf, const unsigned char *data, bufsize_t len);
/** @brief Append a NUL-terminated C string. */
void strbuf_puts(strbuf *buf, const char *str);

void strbuf_copy_cstr(char *dest, bufsize_t dest_size, const strbuf *src);

void strbuf_swap(strbuf *a, strbuf *b);

/** @brief Detach owned storage; caller becomes responsible for free(). */
unsigned char *strbuf_detach(strbuf *buf);

int strbuf_cmp(const strbuf *a, const strbuf *b);

bufsize_t strbuf_strchr(const strbuf *buf, int c, bufsize_t pos);
bufsize_t strbuf_strrchr(const strbuf *buf, int c, bufsize_t pos);

/** @brief Truncate to len bytes if len is smaller than the current size. */
void strbuf_truncate(strbuf *buf, bufsize_t len);
/** @brief Drop n bytes from the front of the buffer. */
void strbuf_drop(strbuf *buf, bufsize_t n);

void strbuf_rtrim(strbuf *buf);
void strbuf_trim(strbuf *buf);
void strbuf_normalize_whitespace(strbuf *buf);
void strbuf_unescape(strbuf *buf);


#ifdef __cplusplus
}
#endif

#endif // __STRBUF_H__

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

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t bufsize_t;
#define BUFSIZE_MAX INT64_MAX

unsigned char strbuf__init_buf[1];

typedef struct strbuf {
  unsigned char *ptr;
  bufsize_t asize, size;
} strbuf;

void strbuf_init(strbuf *buf, bufsize_t init_size);

static void S_strbuf_grow_by(strbuf *buf, bufsize_t grow_size);
void strbuf_grow(strbuf *buf, bufsize_t grow_size);

bufsize_t strbuf_len(const strbuf *buf);

void strbuf_free(strbuf *buf);
void strbuf_clear(strbuf *buf);

void strbuf_set(strbuf *buf, const unsigned char *str, bufsize_t len);
void strbuf_sets(strbuf *buf, const char *str);

void strbuf_putc(strbuf *buf, unsigned char c);
void strbuf_put(strbuf *buf, const unsigned char *data, bufsize_t len);
void strbuf_puts(strbuf *buf, const char *str);

void strbuf_copy_cstr(char *dest, bufsize_t dest_size, const strbuf *src);

void strbuf_swap(strbuf *a, strbuf *b);

unsigned char *strbuf_detach(strbuf *buf);

int strbuf_cmp(const strbuf *a, const strbuf *b);

bufsize_t strbuf_strchr(const strbuf *buf, int c, bufsize_t pos);
bufsize_t strbuf_strrchr(const strbuf *buf, int c, bufsize_t pos);

void strbuf_truncate(strbuf *buf, bufsize_t len);
void strbuf_drop(strbuf *buf, bufsize_t n);

void strbuf_rtrim(strbuf *buf);
void strbuf_trim(strbuf *buf);
void strbuf_normalize_whitespace(strbuf *buf);
void strbuf_unescape(strbuf *buf);


#ifdef __cplusplus
}
#endif

#endif

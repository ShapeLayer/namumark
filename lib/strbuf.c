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

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#ifndef MIN
#define MIN(x, y) ((x < y) ? x : y)
#endif

void strbuf_init(strbuf *buf, bufsize_t init_size) {
  buf->ptr = strbuf__init_buf;
  buf->asize = 0;
  buf->size = 0;

  if (init_size > 0) {
    strbuf_grow(buf, init_size);
  }
}

static void S_strbuf_grow_by(strbuf *buf, bufsize_t grow_size) {
  strbuf_grow(buf, buf->size + grow_size);
}

void strbuf_grow(strbuf *buf, bufsize_t grow_size) {
  assert(grow_size > 0);

  if (grow_size < buf->asize) { return; }

  if (grow_size > (bufsize_t)(BUFSIZE_MAX / 2)) {
    fprintf(stderr, "strbuf_grow: grow_size too large (%lld > %lld), aborting\n", grow_size, BUFSIZE_MAX / 2);
    abort();
  }

  bufsize_t new_size = grow_size + grow_size / 2;
  new_size += 1;
  new_size = (new_size + 7) & ~7;

  buf->ptr = (unsigned char *)realloc(buf->asize ? buf->ptr : NULL, new_size);
  buf->asize = new_size;
}

bufsize_t strbuf_len(const strbuf *buf) { return buf->size; }

void strbuf_free(strbuf *buf) {
  if (!buf) { return; }

  if (buf->ptr != strbuf__init_buf) {
    free(buf->ptr);
  }

  strbuf_init(buf, 0);
}

void strbuf_clear(strbuf *buf) {
  buf->size = 0;

  if (buf->asize > 0) {
    buf->ptr[0] = '\0';
  }
}

void strbuf_set(strbuf *buf, const unsigned char *data, bufsize_t len) {
  if (len <= 0 || data == NULL) {
    strbuf_clear(buf);
  } else {
    if (data != buf->ptr) {
      if (len >= buf->asize) {
        strbuf_grow(buf, len);
      }
      memmove(buf->ptr, data, len);
    }
    buf->size = len;
    buf->ptr[buf->size] = '\0';
  }
}

void strbuf_sets(strbuf *buf, const char *str) {
  strbuf_set(buf, (const unsigned char *)str, str ? (bufsize_t)strlen(str) : 0);
}

void strbuf_putc(strbuf *buf, unsigned char c) {
  S_strbuf_grow_by(buf, 1);
  buf->ptr[buf->size++] = c;
  buf->ptr[buf->size] = '\0';
}

void strbuf_put(strbuf *buf, const unsigned char *data, bufsize_t len) {
  if (len <= 0) { return; }

  S_strbuf_grow_by(buf, len);
  memmove(buf->ptr + buf->size, data, len);
  buf->size += len;
  buf->ptr[buf->size] = '\0';
}

void strbuf_puts(strbuf *buf, const char *str) {
  strbuf_put(buf, (const unsigned char *)str, (bufsize_t)strlen(str));
}

void strbuf_copy_cstr(char *dest, bufsize_t dest_size, const strbuf *src) {
  assert(src);
  if (!dest || dest_size <= 0) { return; }

  if (src->size == 0 || src->asize <= 0) {
    dest[0] = '\0';
    return;
  }

  bufsize_t copylen = src->size;
  if (copylen > dest_size - 1) {
    copylen = dest_size - 1;
  }
  memmove(dest, src->ptr, copylen);
  dest[copylen] = '\0';
}

void strbuf_swap(strbuf *a, strbuf *b) {
  strbuf tmp = *a;
  *a = *b;
  *b = tmp;
}

unsigned char *strbuf_detach(strbuf *buf) {
  unsigned char *data = buf->ptr;

  if (buf->asize == 0) {
    return (unsigned char *)calloc(1, 1);
  }

  strbuf_init(buf, 0);
  return data;
}

int strbuf_cmp(const strbuf *a, const strbuf *b) {
  int result = memcmp(a->ptr, b->ptr, MIN(a->size, b->size));
  if (result != 0) { return result; }
  if (a -> size < b -> size) { return -1; }
  if (a -> size > b -> size) { return 1; }
  return 0;
}

bufsize_t strbuf_strchr(const strbuf *buf, int c, bufsize_t pos) {
  if (pos >= buf->size) { return -1; }
  if (pos < 0) { pos = 0; }

  const unsigned char *p = (unsigned char *)memchr(buf->ptr + pos, c, buf->size - pos);
  if (!p) { return -1; }

  return (bufsize_t)(p - buf->ptr);
}

bufsize_t strbuf_strrchr(const strbuf *buf, int c, bufsize_t pos) {
  if (pos < 0 || buf->size == 0) { return -1; }
  if (pos >= buf->size) { pos = buf->size - 1; }

  bufsize_t i;
  for (i = pos; i >= 0; i--) {
    if (buf->ptr[i] == (unsigned char)c) {
      return i;
    }
  }

  return -1;
}

void strbuf_truncate(strbuf *buf, bufsize_t len) {
  if (len < 0) { len = 0; }

  if (len < buf->size) {
    buf->ptr[len] = '\0';
    buf->size = len;
  }
}

void strbuf_drop(strbuf *buf, bufsize_t n) {
  if (n > 0) {
    if (n > buf->size) { n = buf->size; }
    buf->size = buf->size - n;
    if (buf->size) {
      memmove(buf->ptr, buf->ptr + n, buf->size);
    }
    buf->ptr[buf->size] = '\0';
  }
}

void strbuf_rtrim(strbuf *buf) {
  if (!buf->size) { return; }

  while (buf->size > 0) {
    if (!isspace(buf->ptr[buf->size - 1])) { break; }
    buf->size--;
  }

  buf->ptr[buf->size] = '\0';
}

void strbuf_trim(strbuf *buf) {
  bufsize_t i = 0;

  if (!buf->size) { return; }

  while (i < buf->size && isspace(buf->ptr[i])) { i++; }

  strbuf_drop(buf, i);
  strbuf_rtrim(buf);
}

void strbuf_normalize_whitespace(strbuf *buf) {
  bool last_char_was_space = false;
  bufsize_t r, w;

  for (r = 0, w = 0; r < buf->size; r++) {
    if (isspace(buf->ptr[r])) {
      if (!last_char_was_space && w > 0) {
        buf->ptr[w++] = ' ';
        last_char_was_space = true;
      }
    } else {
      buf->ptr[w++] = buf->ptr[r];
      last_char_was_space = false;
    }
  }

  if (w > 0 && buf->ptr[w - 1] == ' ') {
    w--;
  }

  strbuf_truncate(buf, w);
}

void strbuf_unescape(strbuf *buf) {
  bufsize_t r, w;
  for (r = 0, w = 0; r < buf->size; r++) {
    if (buf->ptr[r] == '\\' && ispunct(buf->ptr[r + 1])) { r++; }
    buf->ptr[w++] = buf->ptr[r];
  }

  strbuf_truncate(buf, w);
}

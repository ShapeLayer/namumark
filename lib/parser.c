/**
 * @file parser.c
 * @brief Streaming input driver that turns bytes into normalized lines.
 *
 * Block parsing is line-oriented, but callers may feed arbitrary byte chunks.
 * This file owns CR/LF normalization, NUL replacement, and parser lifecycle so
 * blocks.c can reason about one complete logical line at a time.
 */
#include <memory.h>
#include <stdlib.h>

#include "blocks.h"
#include "inlines.h"
#include "parser.h"
#include "strbuf.h"

static void parser_dispose(namumark_parser *parser) {
  if (parser == NULL) {
    return;
  }

  if (parser->root != NULL) {
    namumark_node_free(parser->root);
    parser->root = NULL;
    parser->current = NULL;
  }

  strbuf_free(&parser->current_line);
}

void parser_reset(namumark_parser *parser) {
  if (parser == NULL) {
    return;
  }

  parser_dispose(parser);

  memset(parser, 0, sizeof(namumark_parser));
  strbuf_init(&parser->current_line, 256);

  namumark_node *document = make_document();
  parser->root = document;
  parser->current = document;
}

namumark_parser *parser_new(void) {
  namumark_parser *parser = (namumark_parser *)calloc(1, sizeof(namumark_parser));
  if (parser == NULL) {
    return NULL;
  }

  parser_reset(parser);
  return parser;
}

static void S_parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len) {
  const unsigned char *end = buffer + len;

  if (len > BUFSIZE_MAX - parser->total_size) {
    parser->total_size = BUFSIZE_MAX;
  } else {
    parser->total_size += len;
  }

  if (len == 0) {
    return;
  }

  /* A CRLF pair can be split across parser_feed() calls. */
  if (parser->last_buffer_endded_with_cr && *buffer == '\n') {
    buffer++;
  }
  parser->last_buffer_endded_with_cr = false;

  while (buffer < end) {
    const unsigned char *eol = buffer;
    while (eol < end && !is_line_end_char(*eol) && *eol != '\0') {
      eol++;
    }

    bufsize_t chunk_len = (bufsize_t)(eol - buffer);
    if (chunk_len > 0) {
      strbuf_put(&parser->current_line, buffer, chunk_len);
    }

    if (eol < end) {
      unsigned char ch = *eol;
      if (ch == '\0') {
        /* NamuMark is text; replace embedded NUL with U+FFFD rather than truncating. */
        strbuf_puts(&parser->current_line, "\xEF\xBF\xBD");
        buffer = eol + 1;
        continue;
      }

      strbuf_putc(&parser->current_line, ch);
      process_line(parser);

      buffer = eol + 1;
      if (ch == '\r') {
        if (buffer == end) {
          parser->last_buffer_endded_with_cr = true;
        } else if (*buffer == '\n') {
          buffer++;
        }
      }
      continue;
    }

    buffer = eol;
  }
}

void parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len) {
  if (parser == NULL || buffer == NULL || len == 0) {
    return;
  }

  S_parser_feed(parser, buffer, len);
}

namumark_node *parser_finish(namumark_parser *parser) {
  if (parser == NULL || parser->root == NULL) {
    return NULL;
  }

  if (parser->current_line.size > 0) {
    process_line(parser);
  }

  finalize_document(parser);

  namumark_node *document = parser->root;
  parser->root = NULL;
  parser->current = NULL;

  strbuf_free(&parser->current_line);
  memset(&parser->current_line, 0, sizeof(strbuf));

  parser_reset(parser);
  return document;
}

void parser_free(namumark_parser *parser) {
  if (parser == NULL) {
    return;
  }

  parser_dispose(parser);
  free(parser);
}

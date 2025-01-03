#include <memory.h>

#include "blocks.h"
#include "inlines.h"
#include "parser.h"
#include "strbuf.h"

static void parser_dispose(namumark_parser *parser) {}

void parser_reset(namumark_parser *parser) {
  parser_dispose(parser);

  memset(parser, 0, sizeof(namumark_parser));;
  strbuf_init(&parser->current_line, 256);
  namumark_node *document = make_document();

  parser->root = document;
  parser->current = document;
}

namumark_parser *parser_new() {
  namumark_parser *parser = (namumark_parser *)calloc(1, sizeof(namumark_parser));
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

  if (parser->last_buffer_endded_with_cr && *buffer == '\n') {
    buffer++;
  }
  parser->last_buffer_endded_with_cr = false;

  while (buffer < end) {
    const unsigned char *eol;
    bufsize_t chunk_len;
    bool process = false;

    for (eol = buffer; eol < end; eol++) {
      if (is_line_end_char(*eol)) {
        process = true;
        break;
      }
      if (*eol == '\0' && eol < end ) { break; }
    }
    if (eol >= end/* && eof*/) { process = true; }

    chunk_len = (bufsize_t)(eol - buffer);
    if (process) {
      if (parser->current_line.size > 0) {
        strbuf_put(&parser->current_line, buffer, chunk_len);
        // process_line is not implemented
        // S_process_line(parser, parser->current_line.data, parser->current_line.size);
        strbuf_clear(&parser->current_line);
      } else {
        // process_line is not implemented
        // S_process_line(parser, buffer, chunk_len);
      }
    } else {
      if (eol < end && *eol == '\0') {
        strbuf_put(&parser->current_line, buffer, chunk_len);
        // repl is not implemented
        // strbuf_put(&parser->current_line, repl, 3);
      } else {
        strbuf_put(&parser->current_line, buffer, chunk_len);
      }
    }

    buffer += chunk_len;
    if (buffer < end) {
      if (*buffer == '\0') { buffer++; }
      else {
        if (*buffer == 'r') {
          buffer++;
          if (buffer == end) {
            parser->last_buffer_endded_with_cr = true;
          }
        }
        if (buffer < end && *buffer == '\n') {
          buffer++;
        }
      }
    }
  }
}

void parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len) {
  S_parser_feed(parser, buffer, len);
}

namumark_node *parser_finish(namumark_parser *parser) {
  if (parser->root == NULL) { return NULL; }

  if (parser->current_line.size > 0) {
    process_line(parser);
    strbuf_clear(&parser->current_line);
  }

  // finalize

  strbuf_free(&parser->current_line);

  namumark_node *document = parser->root;
  parser->root = NULL;
  parser_reset(parser);
  return document;
  parser->current = NULL;
  parser_dispose(parser);
  return document;
}

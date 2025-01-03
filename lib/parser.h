#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdbool.h>

#include "node.h"
#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct namumark_parser {
  struct namumark_node *root;
  struct namumark_node *current;

  int line_number;
  bufsize_t offset;
  bufsize_t last_line_length;

  size_t total_size;

  bool last_buffer_endded_with_cr;

  strbuf current_line;
} namumark_parser;

static void parser_dispose(namumark_parser *parser);
void parser_reset(namumark_parser *parser);
namumark_parser *parser_new();
static void S_parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len);
void parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif // __PARSER_H__

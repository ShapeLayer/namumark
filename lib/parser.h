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
  bool ignore_remaining_lines;
  bool table_continuation;
  int wiki_block_depth;
  int wiki_nonwiki_depth;
  int advanced_brace_depth;
  int inline_advanced_depth;

  struct namumark_node *wiki_block_node;
  struct namumark_node *advanced_text_node;
  struct namumark_node *inline_text_node;

  strbuf current_line;
} namumark_parser;

void parser_reset(namumark_parser *parser);
namumark_parser *parser_new(void);
void parser_free(namumark_parser *parser);
void parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len);
namumark_node *parser_finish(namumark_parser *parser);

#ifdef __cplusplus
}
#endif

#endif // __PARSER_H__

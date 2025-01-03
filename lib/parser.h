#ifndef __PARSER_H__
#define __PARSER_H__

#include "node.h"
#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct namumark_parser {
  struct namumark_node *root;
  struct namumark_node *current;

  strbuf current_line;
} namumark_parser;

static void parser_dispose(namumark_parser *parser);
void parser_reset(namumark_parser *parser);
namumark_parser *parser_new();

#ifdef __cplusplus
}
#endif

#endif // __PARSER_H__

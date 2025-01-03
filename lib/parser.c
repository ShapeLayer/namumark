#include <memory.h>

#include "blocks.h"
#include "parser.h"

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

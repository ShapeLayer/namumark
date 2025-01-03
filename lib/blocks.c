#include <stdlib.h>

#include "blocks.h"
#include "node.h"
#include "types.h"

static namumark_node *make_block(namumark_node_type node_type, int start_line, int start_column) {
  namumark_node *node = (namumark_node *)calloc(1, sizeof(namumark_node));

  strbuf_init(&node->content, 32);
  node->type = node_type;
  node->start_line = start_line;
  node->start_column = start_column;
  node->end_line = start_line;

  return node;
}

static namumark_node *S_make_document() {
  return make_block(NAMUMARK_NODE_DOCUMENT, 1, 1);
}

namumark_node *make_document() {
  return S_make_document();
}

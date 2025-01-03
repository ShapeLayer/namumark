#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#include "blocks.h"
#include "inlines.h"
#include "node.h"
#include "parser.h"
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

static void S_process_line(namumark_parser *parser) {
  const unsigned char *buffer = parser->current_line.ptr;
  bufsize_t len = parser->current_line.size;
  namumark_node *last_matched_container;
  bool all_matched = true;
  namumark_node *container;
  namumark_node *current;

  // not implemented: validate utf-8
  
  if (len == 0 || !is_line_end_char(buffer[len - 1])) {
    strbuf_putc(&parser->current_line, '\n');
  }

  /*
  parser->offset = 0;
  parser->column = 0;
  parser->first_nonspace = 0;
  parser->first_nonspace_column = 0;
  parser->thematic_break_kill_pos = 0;
  parser->indent = 0;
  parser->blank = false;
  parser->partially_consumed_tab = false;
  */

  if (
    parser->line_number == 0 &&
    parser->current_line.size >= 3 &&
    memcmp(parser->current_line.ptr, "\xEF\xBB\xBF", 3) == 0
  ) {
    parser->offset += 3;
  }

  parser->line_number++;

  // not implemented: check_open_blocks;
  // last_matched_container = check_open_blocks(parser, &all_matched);

  if (last_matched_container) {
    container = last_matched_container;
    current = parser->current;
    // not implemented: open_new_blocks;
    // open_new_blocks(parser, &container, all_matched);
    if (current == parser->current) {
      // not implemented: add_text_to_container;
      // add_text_to_container(container, parser->current_line.ptr, parser->current_line.size);
    }
  }

  parser->last_line_length = parser->current_line.size;
  if (
    parser->last_line_length &&
    parser->current_line.ptr[parser->last_line_length - 1] == '\n'
  ) {
    parser->last_line_length--;
  }
  if (
    parser->last_line_length &&
    parser->current_line.ptr[parser->last_line_length - 1] == '\r'
  ) {
    parser->last_line_length--;
  }

  strbuf_clear(&parser->current_line);
}

void process_line(namumark_parser *parser) {
  S_process_line(parser);
}

static namumark_node *S_finalize(namumark_parser *parser, namumark_node *block) {
  bufsize_t pos;
  namumark_node *item;
  namumark_node *subitem;
  namumark_node *parent;
  bool has_content;

  parent = block->parent;
  assert(block->flags & NAMUMARK_NODE_OPEN);
  block->flags &= ~NAMUMARK_NODE_OPEN;

  if (parser->current_line.size == 0) {
    block->end_line = parser->line_number;
    block->end_column = parser->current_line.size;
  } else if (
    block->type == NAMUMARK_NODE_DOCUMENT
    // other fenced flags must be implemented
  ) {
    block->end_line = parser->line_number;
    block->end_column = parser->current_line.size;
    if (block->end_column) {
      if (is_line_end_char(parser->current_line.ptr[block->end_column - 1])) {
        block->end_column--;
      }
    }
  } else {
    block->end_line = parser->line_number - 1;
    block->end_column = parser->last_line_length;
  }

  strbuf *node_content = &block->content;

  switch (block->type) {
    // not implemented: NAMUMARK_NODE_DOCUMENT
    // not implemented: NAMUMARK_NODE_REDIRECT
    // .. TODO: implement all block types
    default:
      break;
  }
  return parent;
}

namumark_node *finalize(namumark_parser *parser, namumark_node *block) {
  return S_finalize(parser, block);
}

static namumark_node *S_finalize_document(namumark_parser *parser) {
  while (parser->current != parser->root) {
    // not implemented: finalize;
    // S_finalize(parser, parser->current, parser->line_number);
  }

  // not implemented: process_inlines;
  // process_inlines(parser);
  // not implemented: process_footnotes;
  // process_footnotes(parser);

  return parser->root;
}

namumark_node *finalize_document(namumark_parser *parser) {
  return S_finalize_document(parser);
}

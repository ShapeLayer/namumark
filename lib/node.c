#include <stdlib.h>

#include "node.h"

namumark_node *namumark_node_new(namumark_node_type type, int start_line, int start_column) {
  namumark_node *node = (namumark_node *)calloc(1, sizeof(namumark_node));
  if (node == NULL) {
    return NULL;
  }

  strbuf_init(&node->content, 32);
  node->type = type;
  node->start_line = start_line;
  node->start_column = start_column;
  node->end_line = start_line;
  node->end_column = start_column;
  node->flags = NAMUMARK_NODE_OPEN;
  node->list_marker = NAMUMARK_LIST_MARKER_NONE;
  node->link_type = NAMUMARK_LINK_NONE;
  node->advanced_type = NAMUMARK_NODE_ADVANCED_NONE;
  node->macro_type = NAMUMARK_NODE_MACRO_NONE;

  strbuf_init(&node->label, 0);
  strbuf_init(&node->target, 0);
  strbuf_init(&node->args, 0);

  return node;
}

void namumark_node_append_child(namumark_node *parent, namumark_node *child) {
  if (parent == NULL || child == NULL) {
    return;
  }

  child->parent = parent;
  child->prev = parent->last_child;
  child->next = NULL;

  if (parent->last_child != NULL) {
    parent->last_child->next = child;
  } else {
    parent->first_child = child;
  }

  parent->last_child = child;
}

void namumark_node_free(namumark_node *node) {
  if (node == NULL) {
    return;
  }

  namumark_node *child = node->first_child;
  while (child != NULL) {
    namumark_node *next = child->next;
    namumark_node_free(child);
    child = next;
  }

  strbuf_free(&node->content);
  strbuf_free(&node->label);
  strbuf_free(&node->target);
  strbuf_free(&node->args);
  free(node);
}

/**
 * @file node.h
 * @brief AST node representation shared by parser and renderers.
 *
 * Nodes intentionally store several generic string buffers instead of allocating
 * per-node subtype structs.  That keeps the parser simple and lets AST JSON show
 * the same fields for all node kinds, at the cost of documenting which field is
 * meaningful for each node type.
 */
#ifndef __NODE_H__
#define __NODE_H__

#include "strbuf.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct namumark_node {
  /** Raw source content or normalized body text for this node. */
  strbuf content;

  /** Intrusive tree links owned by the document root. */
  struct namumark_node *parent;
  struct namumark_node *prev;
  struct namumark_node *next;
  struct namumark_node *first_child;
  struct namumark_node *last_child;
  namumark_node_type type;

  /** Source span metadata used by diagnostics and AST JSON. */
  int start_line;
  int start_column;
  int end_line;
  int end_column;

  namumark_node_internal_flags flags;

  /** Heading level, list depth, table/list metadata, or macro-specific values. */
  int level;
  int folded;
  int depth;
  int indent;
  int start_number;
  int fixed_comment;
  namumark_list_marker_type list_marker;
  namumark_link_type link_type;
  namumark_node_advanced_type advanced_type;
  namumark_node_macro_type macro_type;

  /** Link labels, CSS class names, or other short display labels. */
  strbuf label;
  /** Link targets, dark-style attributes, or macro target names. */
  strbuf target;
  /** Macro arguments, inline style attributes, or renderer-specific options. */
  strbuf args;
  /** {{{#!wiki onclick="..."}}} action descriptor retained for HTML runtime. */
  strbuf onclick;
  /** {{{#!wiki tag="..."}}} requested output tag; currently only tag="a" is honored. */
  strbuf tag;

  /** Document-level category collection; only meaningful on the root node. */
  strbuf *categories;
  int category_count;
  int category_capacity;
} namumark_node;

/** @brief Allocate a node with initialized string buffers and source position. */
namumark_node *namumark_node_new(namumark_node_type type, int start_line, int start_column);
/** @brief Append child to parent, updating sibling and parent links. */
void namumark_node_append_child(namumark_node *parent, namumark_node *child);
/** @brief Recursively free a node subtree and all owned buffers. */
void namumark_node_free(namumark_node *node);

#ifdef __cplusplus
}
#endif

#endif // __NODE_H__

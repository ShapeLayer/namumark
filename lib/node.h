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

/**
 * @brief A half-open source span in absolute 1-based line/byte-column terms.
 *
 * Same conventions as the node position fields: lines are 1-based, columns are
 * 1-based byte offsets within the physical line, and end_column is one past the
 * span's last byte (width == end_column - start_column). A span with
 * start_line == 0 is "unset" and should be omitted by consumers.
 */
typedef struct namumark_span {
  int start_line;
  int start_column;
  int end_line;
  int end_column;
} namumark_span;

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

  /**
   * Source span metadata used by diagnostics and AST JSON.
   *
   * Lines are 1-based. Columns are 1-based byte offsets within the physical
   * source line (multibyte UTF-8 characters advance the column by their byte
   * length; consumers map these to UTF-16/codepoint offsets as needed).
   *
   * Spans use a half-open convention: end_column is one column past the node's
   * last byte, so width == end_column - start_column and an empty span has
   * end_column == start_column.
   *
   * Columns are absolute line coordinates for every node, including inline
   * children nested inside emphasis, headings, list items, and blockquotes;
   * they are not reset relative to a parent's content buffer.
   *
   * Exception: a node spanning multiple physical lines (a multiline inline
   * advanced block) anchors start_column to its opening line; columns on
   * continuation lines cannot be expressed with a single base, so consumers
   * needing exact multiline spans should split that node's content on '\n'.
   */
  int start_line;
  int start_column;
  int end_line;
  int end_column;

  /**
   * Full source span including markup delimiters, when the node is delimited.
   *
   * The position fields above describe the inner content (text between the
   * delimiters), while outer_span covers the whole syntactic token including
   * the opening/closing punctuation. For example, for '''CD''' the position is
   * the "CD" span and outer_span is the "'''CD'''" span; a highlighting client
   * can paint delimiters by subtracting position from outer_span.
   *
   * outer_span is unset (start_line == 0) for plain text and for nodes without
   * delimiters; in that case consumers should fall back to the position fields.
   */
  namumark_span outer_span;

  /**
   * For link nodes only: absolute spans of the parsed target and (optional)
   * label within the source, splitting around the '|' separator. Unset spans
   * (start_line == 0) mean the component is absent (e.g. a target-only link has
   * no label_span). These describe source positions, not normalized targets.
   */
  namumark_span target_span;
  namumark_span label_span;

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

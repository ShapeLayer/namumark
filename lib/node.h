#ifndef __NODE_H__
#define __NODE_H__

#include "strbuf.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct namumark_node {
  strbuf content;

  struct namumark_node *parent;
  struct namumark_node *prev;
  struct namumark_node *next;
  struct namumark_node *first_child;
  struct namumark_node *last_child;
  namumark_node_type type;

  int start_line;
  int start_column;
  int end_line;
  int end_column;

  namumark_node_internal_flags flags;

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

  strbuf label;
  strbuf target;
  strbuf args;
  strbuf onclick;
  strbuf tag;

  strbuf *categories;
  int category_count;
  int category_capacity;
} namumark_node;

namumark_node *namumark_node_new(namumark_node_type type, int start_line, int start_column);
void namumark_node_append_child(namumark_node *parent, namumark_node *child);
void namumark_node_free(namumark_node *node);

#ifdef __cplusplus
}
#endif

#endif // __NODE_H__

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
} namumark_node;

#ifdef __cplusplus
}
#endif

#endif // __NODE_H__

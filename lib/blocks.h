#ifndef __BLOCKS_H__
#define __BLOCKS_H__

#include <stdlib.h>

#include "node.h"
#include "parser.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

static namumark_node *make_block(namumark_node_type node_type, int start_line, int start_column);
static namumark_node *S_make_document();
namumark_node *make_document();

static void S_process_line(namumark_parser *parser);
void process_line(namumark_parser *parser);

#ifdef __cplusplus
}
#endif

#endif // __BLOCKS_H__

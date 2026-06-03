#ifndef __BLOCKS_H__
#define __BLOCKS_H__

#include <stdlib.h>

#include "node.h"
#include "parser.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

namumark_node *make_document(void);
void process_line(namumark_parser *parser);
namumark_node *finalize(namumark_parser *parser, namumark_node *block);
namumark_node *finalize_document(namumark_parser *parser);

#ifdef __cplusplus
}
#endif

#endif // __BLOCKS_H__

/**
 * @file blocks.h
 * @brief Block-level parser operations used by the streaming parser.
 */
#ifndef __BLOCKS_H__
#define __BLOCKS_H__

#include <stdlib.h>

#include "node.h"
#include "parser.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Create an empty document root node. */
namumark_node *make_document(void);
/** @brief Consume parser->current_line and update the block tree. */
void process_line(namumark_parser *parser);
/** @brief Finalize a block after all continuation lines have been consumed. */
namumark_node *finalize(namumark_parser *parser, namumark_node *block);
/** @brief Finalize the entire document and run inline parsing for pending text. */
namumark_node *finalize_document(namumark_parser *parser);

#ifdef __cplusplus
}
#endif

#endif // __BLOCKS_H__

/**
 * @file renderer.h
 * @brief Internal renderer entry points for AST JSON and HTML output.
 */
#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <stdio.h>

#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Backward-compatible alias for print_document_ast(). */
int print_document(const namumark_node *document, FILE *out);
/** @brief Print the parser tree as stable diagnostic JSON. */
int print_document_ast(const namumark_node *document, FILE *out);
/** @brief Print the document as standalone HTML fragment wrapped in article.namumark. */
int print_document_html(const namumark_node *document, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // __RENDERER_H__

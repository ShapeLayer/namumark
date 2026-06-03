#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <stdio.h>

#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

int print_document(const namumark_node *document, FILE *out);
int print_document_ast(const namumark_node *document, FILE *out);
int print_document_html(const namumark_node *document, FILE *out);

#ifdef __cplusplus
}
#endif

#endif // __RENDERER_H__

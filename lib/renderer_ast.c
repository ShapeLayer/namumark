/**
 * @file renderer_ast.c
 * @brief Diagnostic JSON renderer for parser trees.
 *
 * The AST renderer intentionally prints the same generic fields on every node.
 * That makes diffs verbose, but it lets tests inspect parser decisions without
 * needing subtype-specific JSON schemas.
 */
#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "types.h"

static const char *node_type_name(namumark_node_type type) {
  switch (type) {
    case NAMUMARK_NODE_DOCUMENT:
      return "document";
    case NAMUMARK_NODE_REDIRECT:
      return "redirect";
    case NAMUMARK_NODE_HEADING:
      return "heading";
    case NAMUMARK_NODE_LIST:
      return "list";
    case NAMUMARK_NODE_LIST_ITEM:
      return "list_item";
    case NAMUMARK_NODE_FOOTNOTE_DEFINITION:
      return "footnote_definition";
    case NAMUMARK_NODE_BLOCKQUOTE:
      return "blockquote";
    case NAMUMARK_NODE_HORIZONTAL_RULE:
      return "horizontal_rule";
    case NAMUMARK_NODE_TABLE:
      return "table";
    case NAMUMARK_NODE_CATEGORY:
      return "category";
    case NAMUMARK_NODE_WIKI_BLOCK:
      return "wiki_block";
    case NAMUMARK_NODE_PREFORMATTED:
      return "preformatted";
    case NAMUMARK_NODE_TEXT:
      return "text";
    case NAMUMARK_NODE_BOLD:
      return "bold";
    case NAMUMARK_NODE_ITALIC:
      return "italic";
    case NAMUMARK_NODE_UNDERLINE:
      return "underline";
    case NAMUMARK_NODE_STRIKETHROUGH:
      return "strikethrough";
    case NAMUMARK_NODE_SUPERSCRIPT:
      return "superscript";
    case NAMUMARK_NODE_SUBSCRIPT:
      return "subscript";
    case NAMUMARK_NODE_LINK:
      return "link";
    case NAMUMARK_NODE_IMAGE:
      return "image";
    case NAMUMARK_NODE_VIDEO:
      return "video";
    case NAMUMARK_NODE_FOOTNOTE_REFERENCE:
      return "footnote_reference";
    case NAMUMARK_NODE_MACRO:
      return "macro";
    case NAMUMARK_NODE_COMMENT:
      return "comment";
    case NAMUMARK_NODE_ADVANCED:
      return "advanced";
    default:
      return "unknown";
  }
}

static int print_indent(FILE *out, int depth) {
  for (int i = 0; i < depth; i++) {
    if (fputs("  ", out) < 0) {
      return 0;
    }
  }
  return 1;
}

static int print_quoted(FILE *out, const strbuf *value) {
  if (fputc('"', out) == EOF) {
    return 0;
  }

  if (value != NULL) {
    for (bufsize_t i = 0; i < value->size; i++) {
      unsigned char c = value->ptr[i];
      if (c == '\\' || c == '"') {
        if (fputc('\\', out) == EOF) {
          return 0;
        }
      }

      if (c == '\n') {
        if (fputs("\\n", out) < 0) {
          return 0;
        }
        continue;
      }
      if (c == '\r') {
        if (fputs("\\r", out) < 0) {
          return 0;
        }
        continue;
      }
      if (c == '\t') {
        if (fputs("\\t", out) < 0) {
          return 0;
        }
        continue;
      }

      if (fputc((int)c, out) == EOF) {
        return 0;
      }
    }
  }

  if (fputc('"', out) == EOF) {
    return 0;
  }

  return 1;
}

static int print_node_json(const namumark_node *node, FILE *out, int depth) {
  if (!print_indent(out, depth) || fputs("{\n", out) < 0) {
    return 0;
  }

  if (!print_indent(out, depth + 1) || fputs("\"type\": \"", out) < 0 ||
      fputs(node_type_name(node->type), out) < 0 || fputs("\",\n", out) < 0) {
    return 0;
  }

  /* Source span metadata: lines are 1-based; columns are 1-based byte offsets
   * within the source line. end_column marks the position just past the node's
   * last byte, so it equals start_column for an empty span. */
  if (!print_indent(out, depth + 1) || fputs("\"position\": {", out) < 0) {
    return 0;
  }
  if (fprintf(out, "\"start_line\": %d, \"start_column\": %d, \"end_line\": %d, \"end_column\": %d",
              node->start_line, node->start_column, node->end_line, node->end_column) < 0) {
    return 0;
  }
  if (fputs("},\n", out) < 0) {
    return 0;
  }

  if (!print_indent(out, depth + 1) || fputs("\"content\": ", out) < 0 ||
      !print_quoted(out, &node->content) || fputs(",\n", out) < 0) {
    return 0;
  }

  if (!print_indent(out, depth + 1) || fprintf(out, "\"level\": %d,\n", node->level) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"folded\": %d,\n", node->folded) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"depth\": %d,\n", node->depth) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"indent\": %d,\n", node->indent) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"start_number\": %d,\n", node->start_number) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"fixed_comment\": %d,\n", node->fixed_comment) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"list_marker\": %d,\n", (int)node->list_marker) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"link_type\": %d,\n", (int)node->link_type) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"advanced_type\": %d,\n", (int)node->advanced_type) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fprintf(out, "\"macro_type\": %d,\n", (int)node->macro_type) < 0) {
    return 0;
  }

  if (!print_indent(out, depth + 1) || fputs("\"label\": ", out) < 0 ||
      !print_quoted(out, &node->label) || fputs(",\n", out) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fputs("\"target\": ", out) < 0 ||
      !print_quoted(out, &node->target) || fputs(",\n", out) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fputs("\"args\": ", out) < 0 ||
      !print_quoted(out, &node->args) || fputs(",\n", out) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fputs("\"onclick\": ", out) < 0 ||
      !print_quoted(out, &node->onclick) || fputs(",\n", out) < 0) {
    return 0;
  }
  if (!print_indent(out, depth + 1) || fputs("\"tag\": ", out) < 0 ||
      !print_quoted(out, &node->tag) || fputs(",\n", out) < 0) {
    return 0;
  }

  if (node->type == NAMUMARK_NODE_DOCUMENT) {
    if (!print_indent(out, depth + 1) || fputs("\"categories\": [", out) < 0) {
      return 0;
    }
    for (int i = 0; i < node->category_count; i++) {
      if (i > 0 && fputs(", ", out) < 0) {
        return 0;
      }
      if (!print_quoted(out, &node->categories[i])) {
        return 0;
      }
    }
    if (fputs("],\n", out) < 0) {
      return 0;
    }
  }

  if (!print_indent(out, depth + 1) || fputs("\"children\": [", out) < 0) {
    return 0;
  }

  const namumark_node *child = node->first_child;
  if (child != NULL && fputc('\n', out) == EOF) {
    return 0;
  }

  int first = 1;
  while (child != NULL) {
    if (!first && fputs(",\n", out) < 0) {
      return 0;
    }
    if (!print_node_json(child, out, depth + 2)) {
      return 0;
    }
    first = 0;
    child = child->next;
  }

  if (!first) {
    if (fputc('\n', out) == EOF) {
      return 0;
    }
    if (!print_indent(out, depth + 1)) {
      return 0;
    }
  }

  if (fputs("]\n", out) < 0) {
    return 0;
  }

  if (!print_indent(out, depth) || fputc('}', out) == EOF) {
    return 0;
  }
  return 1;
}

int print_document_ast(const namumark_node *document, FILE *out) {
  if (document == NULL || out == NULL) {
    return 0;
  }

  if (!print_node_json(document, out, 0)) {
    return 0;
  }

  if (fputc('\n', out) == EOF) {
    return 0;
  }

  return 1;
}

int print_document(const namumark_node *document, FILE *out) {
  return print_document_ast(document, out);
}

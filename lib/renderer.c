#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inlines.h"
#include "parser.h"
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

static int print_html_escaped(FILE *out, const strbuf *value) {
  if (out == NULL) {
    return 0;
  }

  if (value == NULL) {
    return 1;
  }

  for (bufsize_t i = 0; i < value->size; i++) {
    unsigned char c = value->ptr[i];
    switch (c) {
      case '&':
        if (fputs("&amp;", out) < 0) {
          return 0;
        }
        break;
      case '<':
        if (fputs("&lt;", out) < 0) {
          return 0;
        }
        break;
      case '>':
        if (fputs("&gt;", out) < 0) {
          return 0;
        }
        break;
      case '"':
        if (fputs("&quot;", out) < 0) {
          return 0;
        }
        break;
      case '\'':
        if (fputs("&#39;", out) < 0) {
          return 0;
        }
        break;
      default:
        if (fputc((int)c, out) == EOF) {
          return 0;
        }
        break;
    }
  }

  return 1;
}

static int render_inline_node(FILE *out, const namumark_node *node);
static int render_block_node(FILE *out, const namumark_node *node);
static int render_block_children(FILE *out, const namumark_node *parent);

static int is_list_continuation_text(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_TEXT && node->content.size > 0 &&
         node->content.ptr[0] == ' ';
}

static int is_rendered_list_sequence_node(const namumark_node *node) {
  return node != NULL && (node->type == NAMUMARK_NODE_LIST || is_list_continuation_text(node));
}

static int count_leading_spaces_in_node(const namumark_node *node) {
  int spaces = 0;
  while ((bufsize_t)spaces < node->content.size && node->content.ptr[spaces] == ' ') {
    spaces++;
  }
  return spaces;
}

static int render_inline_children(FILE *out, const namumark_node *parent) {
  if (parent->first_child == NULL) {
    return print_html_escaped(out, &parent->content);
  }

  const namumark_node *child = parent->first_child;
  while (child != NULL) {
    if (!render_inline_node(out, child)) {
      return 0;
    }
    child = child->next;
  }

  return 1;
}

static int render_inline_wrapped(FILE *out, const char *open, const char *close,
                                 const namumark_node *node) {
  if (fputs(open, out) < 0) {
    return 0;
  }
  if (!render_inline_children(out, node)) {
    return 0;
  }
  if (fputs(close, out) < 0) {
    return 0;
  }
  return 1;
}

static int render_advanced_content(FILE *out, const strbuf *content) {
  if (content == NULL) {
    return 1;
  }

  namumark_node fake_parent = {0};
  strbuf_init(&fake_parent.content, content->size + 1);
  strbuf_set(&fake_parent.content, content->ptr, content->size);
  parse_inlines(content, &fake_parent, 0);

  int ok = render_inline_children(out, &fake_parent);

  strbuf_free(&fake_parent.content);
  namumark_node *child = fake_parent.first_child;
  while (child != NULL) {
    namumark_node *next = child->next;
    namumark_node_free(child);
    child = next;
  }

  return ok;
}

static int has_nested_link_markup(const strbuf *value) {
  if (value == NULL || value->size < 2) {
    return 0;
  }

  for (bufsize_t i = 0; i + 1 < value->size; i++) {
    if (value->ptr[i] == '[' && value->ptr[i + 1] == '[') {
      return 1;
    }
  }

  return 0;
}

static int render_raw_bytes(FILE *out, const unsigned char *data, bufsize_t len) {
  for (bufsize_t i = 0; i < len; i++) {
    if (fputc((int)data[i], out) == EOF) {
      return 0;
    }
  }
  return 1;
}

static int render_html_entity_or_text(FILE *out, const unsigned char *data, bufsize_t len) {
  if (len == 0) {
    return 1;
  }

  for (bufsize_t i = 0; i < len; i++) {
    unsigned char c = data[i];
    if (c == '&') {
      if (i + 1 < len && data[i + 1] == '#') {
        bufsize_t j = i + 2;
        int is_hex = 0;
        if (j < len && (data[j] == 'x' || data[j] == 'X')) {
          is_hex = 1;
          j++;
        }

        bufsize_t digits_start = j;
        while (j < len) {
          unsigned char d = data[j];
          int ok = is_hex ? ((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') ||
                             (d >= 'A' && d <= 'F'))
                          : (d >= '0' && d <= '9');
          if (!ok) {
            break;
          }
          j++;
        }

        if (j > digits_start) {
          if (j < len && data[j] == ';') {
            j++;
          }
          if (!render_raw_bytes(out, data + i, j - i)) {
            return 0;
          }
          i = j - 1;
          continue;
        }
      }
      if (fputs("&amp;", out) < 0) {
        return 0;
      }
      continue;
    }
    if (c == '<') {
      if (fputs("&lt;", out) < 0) {
        return 0;
      }
      continue;
    }
    if (c == '>') {
      if (fputs("&gt;", out) < 0) {
        return 0;
      }
      continue;
    }
    if (c == '"') {
      if (fputs("&quot;", out) < 0) {
        return 0;
      }
      continue;
    }
    if (c == '\'') {
      if (fputs("&#39;", out) < 0) {
        return 0;
      }
      continue;
    }
    if (fputc((int)c, out) == EOF) {
      return 0;
    }
  }

  return 1;
}

static int is_hex_color_token(const unsigned char *s, bufsize_t len) {
  if (!(len == 3 || len == 6)) {
    return 0;
  }
  for (bufsize_t i = 0; i < len; i++) {
    unsigned char c = s[i];
    int is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!is_hex) {
      return 0;
    }
  }
  return 1;
}

static void build_css_color_value(const unsigned char *token, bufsize_t len, strbuf *out) {
  strbuf_clear(out);
  if (token == NULL || len <= 0) {
    return;
  }

  const unsigned char *p = token;
  bufsize_t n = len;
  if (n > 0 && p[0] == '#') {
    p++;
    n--;
  }
  if (n <= 0) {
    return;
  }

  if (is_hex_color_token(p, n)) {
    strbuf_putc(out, '#');
    strbuf_put(out, p, n);
    return;
  }

  strbuf_put(out, p, n);
}

static void split_color_pair(const strbuf *head, strbuf *light, strbuf *dark) {
  strbuf_clear(light);
  strbuf_clear(dark);

  if (head == NULL || head->size <= 0) {
    return;
  }

  bufsize_t comma = strbuf_strchr(head, ',', 0);
  if (comma < 0) {
    build_css_color_value(head->ptr, head->size, light);
    return;
  }

  build_css_color_value(head->ptr, comma, light);
  if (comma + 1 < head->size) {
    build_css_color_value(head->ptr + comma + 1, head->size - comma - 1, dark);
  }
}

static int style_has_inline_display(const strbuf *style) {
  if (style == NULL || style->size <= 0) {
    return 0;
  }

  for (bufsize_t i = 0; i + 7 <= style->size; i++) {
    if (tolower(style->ptr[i]) != 'd' || tolower(style->ptr[i + 1]) != 'i' ||
        tolower(style->ptr[i + 2]) != 's' || tolower(style->ptr[i + 3]) != 'p' ||
        tolower(style->ptr[i + 4]) != 'l' || tolower(style->ptr[i + 5]) != 'a' ||
        tolower(style->ptr[i + 6]) != 'y') {
      continue;
    }

    bufsize_t j = i + 7;
    while (j < style->size && (style->ptr[j] == ' ' || style->ptr[j] == '\t')) {
      j++;
    }
    if (j >= style->size || style->ptr[j] != ':') {
      continue;
    }

    j++;
    while (j < style->size && (style->ptr[j] == ' ' || style->ptr[j] == '\t')) {
      j++;
    }
    if (j + 6 > style->size) {
      continue;
    }

    if (tolower(style->ptr[j]) == 'i' && tolower(style->ptr[j + 1]) == 'n' &&
        tolower(style->ptr[j + 2]) == 'l' && tolower(style->ptr[j + 3]) == 'i' &&
        tolower(style->ptr[j + 4]) == 'n' && tolower(style->ptr[j + 5]) == 'e') {
      return 1;
    }
  }

  return 0;
}

static int render_wiki_advanced_block(FILE *out, const namumark_node *node) {
  int inline_mode = style_has_inline_display(&node->args);

  if (fputs(inline_mode ? "<span class=\"nm-wiki-block\"" : "<div class=\"nm-wiki-block\"",
            out) < 0) {
    return 0;
  }
  if (node->args.size > 0) {
    if (fputs(" style=\"", out) < 0 || !print_html_escaped(out, &node->args) ||
        fputs("\"", out) < 0) {
      return 0;
    }
  }
  if (fputs(">", out) < 0) {
    return 0;
  }

  strbuf body;
  strbuf_init(&body, node->content.size + 1);
  bufsize_t start = 6;
  while (start < node->content.size &&
         (node->content.ptr[start] == ' ' || node->content.ptr[start] == '\t')) {
    start++;
  }
  if (start + 6 <= node->content.size && memcmp(node->content.ptr + start, "style=", 6) == 0) {
    start += 6;
    if (start < node->content.size && node->content.ptr[start] == '"') {
      start++;
      while (start < node->content.size && node->content.ptr[start] != '"') {
        start++;
      }
      if (start < node->content.size && node->content.ptr[start] == '"') {
        start++;
      }
    } else {
      while (start < node->content.size && node->content.ptr[start] != ' ' &&
             node->content.ptr[start] != '\t' && node->content.ptr[start] != '\n' &&
             node->content.ptr[start] != '\r') {
        start++;
      }
    }
  }

  while (start < node->content.size &&
         (node->content.ptr[start] == ' ' || node->content.ptr[start] == '\t')) {
    start++;
  }
  while (start < node->content.size &&
         (node->content.ptr[start] == '\n' || node->content.ptr[start] == '\r')) {
    start++;
  }

  if (start < node->content.size) {
    strbuf_set(&body, node->content.ptr + start, node->content.size - start);
  }

  if (inline_mode) {
    int ok = render_advanced_content(out, &body);
    strbuf_free(&body);
    if (!ok) {
      return 0;
    }
    return fputs("</span>", out) >= 0;
  }

  namumark_parser *sub = parser_new();
  if (sub == NULL) {
    strbuf_free(&body);
    return 0;
  }

  parser_feed(sub, body.ptr, (size_t)body.size);
  namumark_node *doc = parser_finish(sub);
  parser_free(sub);
  strbuf_free(&body);

  if (doc == NULL) {
    return 0;
  }

  if (!render_block_children(out, doc)) {
    namumark_node_free(doc);
    return 0;
  }
  namumark_node_free(doc);

  return fputs("</div>", out) >= 0;
}

static int render_inline_node(FILE *out, const namumark_node *node) {
  switch (node->type) {
    case NAMUMARK_NODE_TEXT:
      return print_html_escaped(out, &node->content);
    case NAMUMARK_NODE_BOLD:
      return render_inline_wrapped(out, "<strong>", "</strong>", node);
    case NAMUMARK_NODE_ITALIC:
      return render_inline_wrapped(out, "<em>", "</em>", node);
    case NAMUMARK_NODE_UNDERLINE:
      return render_inline_wrapped(out, "<u>", "</u>", node);
    case NAMUMARK_NODE_STRIKETHROUGH:
      return render_inline_wrapped(out, "<del>", "</del>", node);
    case NAMUMARK_NODE_SUPERSCRIPT:
      return render_inline_wrapped(out, "<sup>", "</sup>", node);
    case NAMUMARK_NODE_SUBSCRIPT:
      return render_inline_wrapped(out, "<sub>", "</sub>", node);
    case NAMUMARK_NODE_LINK:
      if (fputs("<a href=\"", out) < 0 || !print_html_escaped(out, &node->target) ||
          fputs("\">", out) < 0) {
        return 0;
      }
      if (node->args.size > 0) {
        if (has_nested_link_markup(&node->args)) {
          if (!print_html_escaped(out, &node->args)) {
            return 0;
          }
        } else if (!render_advanced_content(out, &node->args)) {
          return 0;
        }
      } else if (!render_inline_children(out, node)) {
        return 0;
      }
      return fputs("</a>", out) >= 0;
    case NAMUMARK_NODE_MACRO:
      if (node->macro_type == NAMUMARK_NODE_MACRO_BREAKLINE) {
        return fputs("<br />", out) >= 0;
      }
      if (fputs("<span class=\"nm-macro\" data-name=\"", out) < 0 ||
          !print_html_escaped(out, &node->target) || fputs("\">", out) < 0 ||
          !print_html_escaped(out, &node->content) || fputs("</span>", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_ADVANCED:
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_LITERAL) {
        if (fputs("<code>", out) < 0 || !print_html_escaped(out, &node->content) ||
            fputs("</code>", out) < 0) {
          return 0;
        }
        return 1;
      }
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_HTML) {
        bufsize_t start = 6;
        while (start < node->content.size &&
               (node->content.ptr[start] == ' ' || node->content.ptr[start] == '\t')) {
          start++;
        }
        return render_html_entity_or_text(out, node->content.ptr + start,
                                          node->content.size - start);
      }
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_WIKI) {
        return render_wiki_advanced_block(out, node);
      }
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_SIZING ||
          node->advanced_type == NAMUMARK_NODE_ADVANCED_COLOR) {
        bufsize_t split = strbuf_strchr(&node->content, ' ', 0);
        if (split > 0 && split + 1 < node->content.size) {
          strbuf head;
          strbuf body;
          strbuf_init(&head, split + 1);
          strbuf_init(&body, node->content.size - split);
          strbuf_set(&head, node->content.ptr, split);
          strbuf_set(&body, node->content.ptr + split + 1, node->content.size - split - 1);

          if (fputs("<span class=\"nm-advanced\" data-advanced=\"", out) < 0 ||
              !print_html_escaped(out, &head) || fputs("\"", out) < 0) {
            strbuf_free(&head);
            strbuf_free(&body);
            return 0;
          }

          strbuf light_color;
          strbuf dark_color;
          strbuf_init(&light_color, 24);
          strbuf_init(&dark_color, 24);

          if (node->advanced_type == NAMUMARK_NODE_ADVANCED_COLOR) {
            split_color_pair(&head, &light_color, &dark_color);
            if (light_color.size > 0) {
              if (fputs(" style=\"color:", out) < 0 || !print_html_escaped(out, &light_color) ||
                  fputs(";\"", out) < 0) {
                strbuf_free(&light_color);
                strbuf_free(&dark_color);
                strbuf_free(&head);
                strbuf_free(&body);
                return 0;
              }
            }
            if (dark_color.size > 0) {
              if (fputs(" data-dark-style=\"color:", out) < 0 ||
                  !print_html_escaped(out, &dark_color) || fputs(";\"", out) < 0) {
                strbuf_free(&light_color);
                strbuf_free(&dark_color);
                strbuf_free(&head);
                strbuf_free(&body);
                return 0;
              }
            }
          }

          if (fputs(">", out) < 0) {
            strbuf_free(&light_color);
            strbuf_free(&dark_color);
            strbuf_free(&head);
            strbuf_free(&body);
            return 0;
          }

          if (!render_advanced_content(out, &body) || fputs("</span>", out) < 0) {
            strbuf_free(&light_color);
            strbuf_free(&dark_color);
            strbuf_free(&head);
            strbuf_free(&body);
            return 0;
          }

          strbuf_free(&light_color);
          strbuf_free(&dark_color);
          strbuf_free(&head);
          strbuf_free(&body);
          return 1;
        }
      }
      if (fputs("<span class=\"nm-advanced\">", out) < 0 ||
          !print_html_escaped(out, &node->content) || fputs("</span>", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_COMMENT:
      if (fputs("<!--", out) < 0) {
        return 0;
      }
      if (node->fixed_comment && fputs("@", out) < 0) {
        return 0;
      }
      if (!print_html_escaped(out, &node->content) || fputs("-->", out) < 0) {
        return 0;
      }
      return 1;
    default:
      return print_html_escaped(out, &node->content);
  }
}

static int is_row_start_line(const unsigned char *line, bufsize_t len) {
  bufsize_t s = 0;
  while (s < len && line[s] == ' ') {
    s++;
  }
  return (s + 1 < len && line[s] == '|' && line[s + 1] == '|');
}

typedef struct table_render_style {
  strbuf align;
  strbuf width;
  strbuf bgcolor;
  strbuf color;
  int colspan;
  int rowspan;
  int no_padding;
} table_render_style;

typedef struct table_render_context {
  strbuf width;
  strbuf bgcolor;
  strbuf bordercolor;
  strbuf align;
} table_render_context;

static void table_style_init(table_render_style *style) {
  strbuf_init(&style->align, 16);
  strbuf_init(&style->width, 32);
  strbuf_init(&style->bgcolor, 32);
  strbuf_init(&style->color, 32);
  style->colspan = 0;
  style->rowspan = 0;
  style->no_padding = 0;
}

static void table_style_free(table_render_style *style) {
  strbuf_free(&style->align);
  strbuf_free(&style->width);
  strbuf_free(&style->bgcolor);
  strbuf_free(&style->color);
}

static void table_context_init(table_render_context *ctx) {
  strbuf_init(&ctx->width, 32);
  strbuf_init(&ctx->bgcolor, 32);
  strbuf_init(&ctx->bordercolor, 32);
  strbuf_init(&ctx->align, 16);
}

static void table_context_free(table_render_context *ctx) {
  strbuf_free(&ctx->width);
  strbuf_free(&ctx->bgcolor);
  strbuf_free(&ctx->bordercolor);
  strbuf_free(&ctx->align);
}

static void set_primary_value(strbuf *dest, const unsigned char *value, bufsize_t len) {
  bufsize_t take = 0;
  while (take < len && value[take] != ',') {
    take++;
  }
  strbuf_set(dest, value, take);
}

static int parse_positive_int(const unsigned char *value, bufsize_t len) {
  int n = 0;
  if (len <= 0) {
    return 0;
  }
  for (bufsize_t i = 0; i < len; i++) {
    if (!isdigit(value[i])) {
      return 0;
    }
    n = n * 10 + (value[i] - '0');
    if (n > 1000) {
      return 1000;
    }
  }
  return n;
}

static void parse_table_token(const unsigned char *token, bufsize_t token_len,
                              table_render_context *table_style,
                              table_render_style *row_style,
                              table_render_style *cell_style) {
  if (token_len <= 0) {
    return;
  }

  if (token_len == 1) {
    if (token[0] == ':') {
      strbuf_set(&cell_style->align, (const unsigned char *)"center", 6);
      return;
    }
    if (token[0] == '(') {
      strbuf_set(&cell_style->align, (const unsigned char *)"left", 4);
      return;
    }
    if (token[0] == ')') {
      strbuf_set(&cell_style->align, (const unsigned char *)"right", 5);
      return;
    }
  }

  if (token[0] == '-') {
    int span = parse_positive_int(token + 1, token_len - 1);
    if (span > 0) {
      cell_style->colspan = span;
    }
    return;
  }

  if (token[0] == '|') {
    int span = parse_positive_int(token + 1, token_len - 1);
    if (span > 0) {
      cell_style->rowspan = span;
    }
    return;
  }

  if (token_len == 5 && memcmp(token, "nopad", 5) == 0) {
    cell_style->no_padding = 1;
    return;
  }

  const unsigned char *eq = (const unsigned char *)memchr(token, '=', (size_t)token_len);
  if (eq == NULL) {
    return;
  }

  bufsize_t key_len = (bufsize_t)(eq - token);
  const unsigned char *val = eq + 1;
  bufsize_t val_len = token_len - key_len - 1;

  if (key_len == 5 && memcmp(token, "width", 5) == 0) {
    strbuf_set(&cell_style->width, val, val_len);
    return;
  }
  if (key_len == 7 && memcmp(token, "bgcolor", 7) == 0) {
    set_primary_value(&cell_style->bgcolor, val, val_len);
    return;
  }
  if (key_len == 5 && memcmp(token, "color", 5) == 0) {
    set_primary_value(&cell_style->color, val, val_len);
    return;
  }
  if (key_len == 5 && memcmp(token, "align", 5) == 0) {
    if (val_len == 6 && memcmp(val, "center", 6) == 0) {
      strbuf_set(&cell_style->align, (const unsigned char *)"center", 6);
    } else if (val_len == 6 && memcmp(val, "middle", 6) == 0) {
      strbuf_set(&cell_style->align, (const unsigned char *)"center", 6);
    } else if (val_len == 5 && memcmp(val, "right", 5) == 0) {
      strbuf_set(&cell_style->align, (const unsigned char *)"right", 5);
    } else {
      strbuf_set(&cell_style->align, (const unsigned char *)"left", 4);
    }
    return;
  }

  if (key_len == 8 && memcmp(token, "rowcolor", 8) == 0) {
    set_primary_value(&row_style->color, val, val_len);
    return;
  }
  if (key_len == 10 && memcmp(token, "rowbgcolor", 10) == 0) {
    set_primary_value(&row_style->bgcolor, val, val_len);
    return;
  }

  if (key_len == 10 && memcmp(token, "tablewidth", 10) == 0) {
    strbuf_set(&table_style->width, val, val_len);
    return;
  }
  if (key_len == 12 && memcmp(token, "tablebgcolor", 12) == 0) {
    set_primary_value(&table_style->bgcolor, val, val_len);
    return;
  }
  if (key_len == 16 && memcmp(token, "tablebordercolor", 16) == 0) {
    set_primary_value(&table_style->bordercolor, val, val_len);
    return;
  }
  if (key_len == 10 && memcmp(token, "tablealign", 10) == 0) {
    strbuf_set(&table_style->align, val, val_len);
    return;
  }
}

static void trim_spaces(const strbuf *src, bufsize_t *start, bufsize_t *end) {
  while (*start < *end && src->ptr[*start] == ' ') {
    (*start)++;
  }
  while (*end > *start && src->ptr[*end - 1] == ' ') {
    (*end)--;
  }
}

static void parse_cell_prefix(const strbuf *cell, table_render_context *table_style,
                              table_render_style *row_style,
                              table_render_style *cell_style,
                              bufsize_t *content_start) {
  bufsize_t i = 0;
  while (i < cell->size && cell->ptr[i] == ' ') {
    i++;
  }

  while (i < cell->size && cell->ptr[i] == '<') {
    bufsize_t close = i + 1;
    while (close < cell->size && cell->ptr[close] != '>') {
      close++;
    }
    if (close >= cell->size || cell->ptr[close] != '>') {
      break;
    }

    parse_table_token(cell->ptr + i + 1, close - i - 1, table_style, row_style, cell_style);
    i = close + 1;
    while (i < cell->size && cell->ptr[i] == ' ') {
      i++;
    }
  }

  *content_start = i;
}

static bufsize_t find_table_cell_separator(const unsigned char *line, bufsize_t from,
                                           bufsize_t line_end) {
  int brace_depth = 0;

  for (bufsize_t i = from; i + 1 < line_end; i++) {
    if (i + 2 < line_end && line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      brace_depth++;
      i += 2;
      continue;
    }

    if (i + 2 < line_end && line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (brace_depth > 0) {
        brace_depth--;
      }
      i += 2;
      continue;
    }

    if (brace_depth == 0 && line[i] == '|' && line[i + 1] == '|') {
      return i;
    }
  }

  return -1;
}

static int table_row_ends_with_separator(const strbuf *row) {
  if (row == NULL || row->size < 2) {
    return 0;
  }

  bufsize_t end = row->size;
  while (end > 0 && (row->ptr[end - 1] == ' ' || row->ptr[end - 1] == '\n' ||
                     row->ptr[end - 1] == '\r')) {
    end--;
  }
  if (end < 2) {
    return 0;
  }

  int brace_depth = 0;
  bufsize_t last_sep = -1;
  for (bufsize_t i = 0; i + 1 < end; i++) {
    if (i + 2 < end && row->ptr[i] == '{' && row->ptr[i + 1] == '{' &&
        row->ptr[i + 2] == '{') {
      brace_depth++;
      i += 2;
      continue;
    }

    if (i + 2 < end && row->ptr[i] == '}' && row->ptr[i + 1] == '}' &&
        row->ptr[i + 2] == '}') {
      if (brace_depth > 0) {
        brace_depth--;
      }
      i += 2;
      continue;
    }

    if (brace_depth == 0 && row->ptr[i] == '|' && row->ptr[i + 1] == '|') {
      last_sep = i;
      i++;
    }
  }

  return last_sep + 2 == end;
}

static void collect_table_style_from_row(const unsigned char *line, bufsize_t line_len,
                                         table_render_context *table_style) {
  bufsize_t s = 0;
  while (s < line_len && line[s] == ' ') {
    s++;
  }
  if (s + 1 >= line_len || line[s] != '|' || line[s + 1] != '|') {
    return;
  }

  bufsize_t p = s + 2;
  while (p <= line_len) {
    bufsize_t sep = find_table_cell_separator(line, p, line_len);

    bufsize_t cell_start = p;
    bufsize_t cell_end = (sep >= 0) ? sep : line_len;
    while (cell_start < cell_end && line[cell_start] == ' ') {
      cell_start++;
    }
    while (cell_end > cell_start && line[cell_end - 1] == ' ') {
      cell_end--;
    }

    strbuf cell;
    strbuf_init(&cell, cell_end - cell_start + 1);
    if (cell_end > cell_start) {
      strbuf_set(&cell, line + cell_start, cell_end - cell_start);
    }

    table_render_style row_style;
    table_render_style cell_style;
    table_style_init(&row_style);
    table_style_init(&cell_style);
    bufsize_t content_start = 0;
    parse_cell_prefix(&cell, table_style, &row_style, &cell_style, &content_start);
    table_style_free(&row_style);
    table_style_free(&cell_style);
    strbuf_free(&cell);

    if (sep < 0 || sep + 1 >= line_len) {
      break;
    }
    p = sep + 2;
  }
}

static int is_horizontal_rule_text(const unsigned char *text, bufsize_t len) {
  bufsize_t s = 0;
  while (s < len && text[s] == ' ') {
    s++;
  }
  bufsize_t e = len;
  while (e > s && text[e - 1] == ' ') {
    e--;
  }
  bufsize_t n = e - s;
  if (n < 4 || n > 9) {
    return 0;
  }
  for (bufsize_t i = s; i < e; i++) {
    if (text[i] != '-') {
      return 0;
    }
  }
  return 1;
}

static int render_inline_snippet(FILE *out, const unsigned char *text, bufsize_t len) {
  strbuf part;
  strbuf_init(&part, len + 1);
  if (len > 0) {
    strbuf_set(&part, text, len);
  }

  namumark_node fake_parent = {0};
  strbuf_init(&fake_parent.content, part.size + 1);
  strbuf_set(&fake_parent.content, part.ptr, part.size);
  parse_inlines(&part, &fake_parent, 0);

  int ok = render_inline_children(out, &fake_parent);

  strbuf_free(&fake_parent.content);
  namumark_node *child = fake_parent.first_child;
  while (child != NULL) {
    namumark_node *next = child->next;
    namumark_node_free(child);
    child = next;
  }
  strbuf_free(&part);
  return ok;
}

static int count_token_occurrences(const unsigned char *text, bufsize_t len,
                                   const char *token, bufsize_t token_len) {
  int count = 0;
  if (token_len == 0 || len < token_len) {
    return 0;
  }
  for (bufsize_t i = 0; i + token_len <= len; i++) {
    if (memcmp(text + i, token, (size_t)token_len) == 0) {
      count++;
      i += token_len - 1;
    }
  }
  return count;
}

static bufsize_t find_wiki_advanced_start(const unsigned char *text, bufsize_t start,
                                          bufsize_t end) {
  static const char token[] = "{{{#!wiki";
  static const bufsize_t token_len = sizeof(token) - 1;

  if (end - start < token_len) {
    return -1;
  }

  for (bufsize_t i = start; i + token_len <= end; i++) {
    if (memcmp(text + i, token, (size_t)token_len) != 0) {
      continue;
    }
    if (i > start && text[i - 1] == '{') {
      continue;
    }
    return i;
  }

  return -1;
}

static int render_table_cell_content(FILE *out, const unsigned char *text, bufsize_t len) {
  if (len <= 0) {
    return 1;
  }

  int has_newline = 0;
  for (bufsize_t i = 0; i < len; i++) {
    if (text[i] == '\n') {
      has_newline = 1;
      break;
    }
  }

  if (!has_newline) {
    return render_inline_snippet(out, text, len);
  }

  bufsize_t line_start = 0;
  int first_plain = 1;
  int in_list = 0;
  int in_wiki_advanced = 0;
  int wiki_advanced_depth = 0;
  strbuf wiki_advanced;
  strbuf_init(&wiki_advanced, 128);

  while (line_start <= len) {
    bufsize_t line_end = line_start;
    while (line_end < len && text[line_end] != '\n') {
      line_end++;
    }

    bufsize_t s = line_start;
    while (s < line_end && text[s] == ' ') {
      s++;
    }

    bufsize_t e = line_end;
    while (e > s && (text[e - 1] == ' ' || text[e - 1] == '\r')) {
      e--;
    }

    if (in_wiki_advanced) {
      if (wiki_advanced.size > 0) {
        strbuf_putc(&wiki_advanced, '\n');
      }
      bufsize_t wiki_line_end = line_end;
      if (wiki_line_end > line_start && text[wiki_line_end - 1] == '\r') {
        wiki_line_end--;
      }
      if (wiki_line_end > line_start) {
        strbuf_put(&wiki_advanced, text + line_start, wiki_line_end - line_start);
      }

      wiki_advanced_depth += count_token_occurrences(text + line_start,
                                                      wiki_line_end - line_start, "{{{", 3);
      wiki_advanced_depth -= count_token_occurrences(text + line_start,
                                                      wiki_line_end - line_start, "}}}", 3);

      if (wiki_advanced_depth <= 0) {
        if (!render_inline_snippet(out, wiki_advanced.ptr, wiki_advanced.size)) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        strbuf_clear(&wiki_advanced);
        in_wiki_advanced = 0;
        wiki_advanced_depth = 0;
        first_plain = 0;
      }

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    bufsize_t wiki_start = (e > s) ? find_wiki_advanced_start(text, s, e) : -1;
    if (wiki_start >= 0) {
      if (in_list) {
        if (fputs("</ul>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list = 0;
      }

      if (wiki_start > s) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        if (!render_inline_snippet(out, text + s, wiki_start - s)) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        first_plain = 0;
      }

      if (wiki_advanced.size > 0) {
        strbuf_putc(&wiki_advanced, '\n');
      }
      strbuf_put(&wiki_advanced, text + wiki_start, e - wiki_start);

      int starts = count_token_occurrences(text + wiki_start, e - wiki_start, "{{{", 3);
      int ends = count_token_occurrences(text + wiki_start, e - wiki_start, "}}}", 3);
      wiki_advanced_depth = starts - ends;

      if (wiki_advanced_depth <= 0) {
        if (!render_inline_snippet(out, wiki_advanced.ptr, wiki_advanced.size)) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        strbuf_clear(&wiki_advanced);
        wiki_advanced_depth = 0;
        first_plain = 0;
      } else {
        in_wiki_advanced = 1;
      }

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    int is_list = 0;
    int list_indent = 0;
    bufsize_t list_start = s;

    {
      bufsize_t leading = line_start;
      while (leading < line_end && text[leading] == ' ') {
        leading++;
        list_indent++;
      }
      if (leading < line_end && text[leading] == '*' &&
          (leading + 1 >= line_end || text[leading + 1] == ' ')) {
        is_list = 1;
        list_start = leading + 1;
        if (list_start < line_end && text[list_start] == ' ') {
          list_start++;
        }
      }
    }

    if (is_list) {
      while (in_list > list_indent) {
        if (fputs("</li></ul>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }
      while (in_list < list_indent) {
        if (fputs("<ul><li>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list++;
      }

      if (in_list == 0) {
        if (fputs("<ul>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list = 1;
        if (fputs("<li>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
      } else {
        if (fputs("</li>\n<li>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
      }
      if (!render_inline_snippet(out, text + list_start, e - list_start)) {
        strbuf_free(&wiki_advanced);
        return 0;
      }
    } else {
      while (in_list > 0) {
        if (fputs("</li></ul>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }

      if (is_horizontal_rule_text(text + s, e - s)) {
        if (fputs("<hr>", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
      } else if (e > s) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        if (!render_inline_snippet(out, text + s, e - s)) {
          strbuf_free(&wiki_advanced);
          return 0;
        }
        first_plain = 0;
      }
    }

    if (line_end >= len) {
      break;
    }
    line_start = line_end + 1;
  }

  while (in_list > 0) {
    if (fputs("</li></ul>", out) < 0) {
      strbuf_free(&wiki_advanced);
      return 0;
    }
    in_list--;
  }

  if (wiki_advanced.size > 0) {
    if (!render_inline_snippet(out, wiki_advanced.ptr, wiki_advanced.size)) {
      strbuf_free(&wiki_advanced);
      return 0;
    }
  }

  strbuf_free(&wiki_advanced);

  return 1;
}

static int render_table_row(FILE *out, const strbuf *row, int row_index,
                            table_render_context *table_style) {
  (void)row_index;

  bufsize_t line_end = row->size;
  while (line_end > 0 && (row->ptr[line_end - 1] == '\n' || row->ptr[line_end - 1] == '\r')) {
    line_end--;
  }
  while (line_end > 0 && row->ptr[line_end - 1] == ' ') {
    line_end--;
  }

  bufsize_t s = 0;
  while (s < line_end && row->ptr[s] == ' ') {
    s++;
  }

  if (s + 1 >= line_end || row->ptr[s] != '|' || row->ptr[s + 1] != '|') {
    return 1;
  }

  if (fputs("<tr>", out) < 0) {
    return 0;
  }

  table_render_style row_style;
  table_style_init(&row_style);

  bufsize_t p = s + 2;
  while (p < line_end) {
    bufsize_t sep = find_table_cell_separator(row->ptr, p, line_end);

    bufsize_t cell_start = p;
    bufsize_t cell_end = (sep >= 0) ? sep : line_end;
    while (cell_start < cell_end && row->ptr[cell_start] == ' ') {
      cell_start++;
    }
    while (cell_end > cell_start && row->ptr[cell_end - 1] == ' ') {
      cell_end--;
    }

    strbuf cell;
    strbuf_init(&cell, cell_end - cell_start + 1);
    if (cell_end > cell_start) {
      strbuf_set(&cell, row->ptr + cell_start, cell_end - cell_start);
    }

    table_render_style cell_style;
    table_style_init(&cell_style);
    bufsize_t content_start = 0;
    parse_cell_prefix(&cell, table_style, &row_style, &cell_style, &content_start);

    bufsize_t content_end = cell.size;
    trim_spaces(&cell, &content_start, &content_end);

    const char *tag = "td";
    if (fputs("<", out) < 0 || fputs(tag, out) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    if (cell_style.colspan > 1 && fprintf(out, " colspan=\"%d\"", cell_style.colspan) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }
    if (cell_style.rowspan > 1 && fprintf(out, " rowspan=\"%d\"", cell_style.rowspan) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    if (fputs(" style=\"", out) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    if (cell_style.width.size > 0) {
      if (fputs("width:", out) < 0 || !print_html_escaped(out, &cell_style.width) ||
          fputs(";", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    const strbuf *align = (cell_style.align.size > 0) ? &cell_style.align :
                          (row_style.align.size > 0) ? &row_style.align : NULL;
    if (align != NULL && align->size > 0) {
      if (fputs("text-align:", out) < 0 || !print_html_escaped(out, align) || fputs(";", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    const strbuf *bgcolor = (cell_style.bgcolor.size > 0) ? &cell_style.bgcolor :
                            (row_style.bgcolor.size > 0) ? &row_style.bgcolor : NULL;
    if (bgcolor != NULL && bgcolor->size > 0) {
      if (fputs("background-color:", out) < 0 || !print_html_escaped(out, bgcolor) ||
          fputs(";", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    const strbuf *color = (cell_style.color.size > 0) ? &cell_style.color :
                          (row_style.color.size > 0) ? &row_style.color : NULL;
    if (color != NULL && color->size > 0) {
      if (fputs("color:", out) < 0 || !print_html_escaped(out, color) || fputs(";", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    if (fputs("\">", out) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    if (cell_style.no_padding) {
      if (fputs("<div style=\"padding:0;\">", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    } else {
      if (fputs("<div>", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    if (!render_table_cell_content(out, cell.ptr + content_start, content_end - content_start)) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    table_style_free(&cell_style);
    strbuf_free(&cell);

    if (fputs("</div></", out) < 0 || fputs(tag, out) < 0 || fputs(">", out) < 0) {
      table_style_free(&row_style);
      return 0;
    }

    if (sep < 0 || sep + 1 >= line_end) {
      break;
    }
    p = sep + 2;
  }

  if (fputs("</tr>\n", out) < 0) {
    table_style_free(&row_style);
    return 0;
  }
  table_style_free(&row_style);
  return 1;
}

static int render_block_node(FILE *out, const namumark_node *node) {
  switch (node->type) {
    case NAMUMARK_NODE_REDIRECT:
      if (fputs("<!-- redirect: ", out) < 0 || !print_html_escaped(out, &node->target) ||
          fputs(" -->\n", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_HEADING:
      if (fprintf(out, "<h%d>", node->level > 0 ? node->level : 2) < 0 ||
          !render_inline_children(out, node) || fputs("</h", out) < 0 ||
          fprintf(out, "%d>\n", node->level > 0 ? node->level : 2) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_LIST: {
      /* Collect all consecutive LIST siblings to render as one nested tree */
      const namumark_node *first_list = node;
      /* Gather sibling list nodes that follow immediately */

      /* Helper: render a list-item group with indent depth using a stack-based approach.
         We walk sibling LIST nodes and nest by item->indent. */

      /* Build a flat array of (indent, marker, start_number, item_content_ptr) then tree-render. */
      #define NM_MAX_LIST_ITEMS 512
      typedef struct {
        int indent;
        const namumark_node *item_node;
        const namumark_node *continuation_node;
        const char *tag;
      } nm_list_entry;
      nm_list_entry entries[NM_MAX_LIST_ITEMS];
      int entry_count = 0;

      const namumark_node *walk = first_list;
      while (walk != NULL && is_rendered_list_sequence_node(walk)) {
        if (walk->type == NAMUMARK_NODE_LIST) {
          const namumark_node *item = walk->first_child;
          while (item != NULL) {
            if (item->type == NAMUMARK_NODE_LIST_ITEM && entry_count < NM_MAX_LIST_ITEMS) {
              const char *t = "ul";
              if (item->list_marker != NAMUMARK_LIST_MARKER_BULLET && item->list_marker != NAMUMARK_LIST_MARKER_NONE) {
                t = "ol";
              }
              entries[entry_count].indent = item->indent > 0 ? item->indent : 1;
              entries[entry_count].item_node = item;
              entries[entry_count].continuation_node = NULL;
              entries[entry_count].tag = t;
              entry_count++;
            }
            item = item->next;
          }
        } else if (entry_count > 0) {
          int continuation_indent = count_leading_spaces_in_node(walk);
          int spaces_only = (bufsize_t)continuation_indent >= walk->content.size;
          if (spaces_only || continuation_indent >= entries[entry_count - 1].indent) {
            entries[entry_count].indent = continuation_indent;
            entries[entry_count].item_node = NULL;
            entries[entry_count].continuation_node = walk;
            entries[entry_count].tag = entries[entry_count - 1].tag;
            entry_count++;
          } else {
            break;
          }
        }
        walk = walk->next;
      }

      /* Render with stack tracking current depth */
      int depth_stack[NM_MAX_LIST_ITEMS + 1];
      const char *tag_stack[NM_MAX_LIST_ITEMS + 1];
      int stack_top = 0;

      for (int idx = 0; idx < entry_count; idx++) {
        int target = entries[idx].indent;
        const char *etag = entries[idx].tag;
        const namumark_node *ei = entries[idx].item_node;
        const namumark_node *continuation = entries[idx].continuation_node;

        if (continuation != NULL) {
          int continuation_target = target;
          while (stack_top > 0 && depth_stack[stack_top - 1] > target) {
            if (fprintf(out, "</li></%s>\n", tag_stack[stack_top - 1]) < 0) {
              return 0;
            }
            stack_top--;
          }
          if (stack_top > 0) {
            continuation_target = depth_stack[stack_top - 1];
          }
          if (fputs("<br />", out) < 0) {
            return 0;
          }
          int spaces = count_leading_spaces_in_node(continuation);
          bufsize_t start = spaces >= continuation_target ? (bufsize_t)continuation_target :
                                                            (bufsize_t)spaces;
          strbuf body;
          strbuf_init(&body, continuation->content.size - start + 1);
          if (start < continuation->content.size) {
            strbuf_put(&body, continuation->content.ptr + start,
                       continuation->content.size - start);
          }
          namumark_node fake_parent = {0};
          strbuf_init(&fake_parent.content, body.size + 1);
          strbuf_set(&fake_parent.content, body.ptr, body.size);
          parse_inlines(&body, &fake_parent, continuation->start_line);
          int ok = render_inline_children(out, &fake_parent);
          strbuf_free(&fake_parent.content);
          namumark_node *child = fake_parent.first_child;
          while (child != NULL) {
            namumark_node *next = child->next;
            namumark_node_free(child);
            child = next;
          }
          strbuf_free(&body);
          if (!ok) {
            return 0;
          }
          continue;
        }

        /* Close deeper levels */
        while (stack_top > 0 && depth_stack[stack_top - 1] > target) {
          if (fprintf(out, "</li></%s>\n", tag_stack[stack_top - 1]) < 0) {
            return 0;
          }
          stack_top--;
        }

        if (stack_top == 0 || depth_stack[stack_top - 1] < target) {
          /* Open a new list level */
          if (stack_top > 0) {
            /* This is a deeper nesting, open inside current <li> */
          } else {
            /* Fresh open */
          }
          if (fprintf(out, "<%s>\n", etag) < 0) {
            return 0;
          }
          depth_stack[stack_top] = target;
          tag_stack[stack_top] = etag;
          stack_top++;
          if (fputs("<li>", out) < 0 || !render_inline_children(out, ei)) {
            return 0;
          }
        } else {
          /* Same depth: close previous item, open next */
          if (fprintf(out, "</li>\n<li>") < 0 || !render_inline_children(out, ei)) {
            return 0;
          }
        }
      }

      /* Close all open levels */
      while (stack_top > 0) {
        stack_top--;
        if (fprintf(out, "</li></%s>\n", tag_stack[stack_top]) < 0) {
          return 0;
        }
      }
      #undef NM_MAX_LIST_ITEMS

      /* Skip siblings that were already consumed above */
      /* Caller (render_document) iterates child->next which will skip them properly
         since we don't modify node; but we need to return after consuming multiple.
         Use a static sentinel trick: handled by rendering them flatly; done. */
      return 1;
    }
    case NAMUMARK_NODE_BLOCKQUOTE:
      if (fputs("<blockquote>", out) < 0 || !render_inline_children(out, node) ||
          fputs("</blockquote>\n", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_HORIZONTAL_RULE:
      return fputs("<hr />\n", out) >= 0;
    case NAMUMARK_NODE_TABLE: {
      strbuf tablebuf;
      strbuf_init(&tablebuf, node->content.size + 1);
      strbuf_set(&tablebuf, node->content.ptr, node->content.size);

      bufsize_t line_start = 0;
      int row_index = 0;
      strbuf pending_row;
      strbuf_init(&pending_row, 128);

      table_render_context table_style;
      table_context_init(&table_style);

      {
        bufsize_t probe_start = 0;
        while (probe_start <= tablebuf.size) {
          bufsize_t probe_end = strbuf_strchr(&tablebuf, '\n', probe_start);
          if (probe_end < 0) {
            probe_end = tablebuf.size;
          }

          if (probe_end > probe_start) {
            const unsigned char *line_ptr = tablebuf.ptr + probe_start;
            bufsize_t line_len = probe_end - probe_start;
            if (is_row_start_line(line_ptr, line_len)) {
              collect_table_style_from_row(line_ptr, line_len, &table_style);
              break;
            }
          }

          if (probe_end >= tablebuf.size) {
            break;
          }
          probe_start = probe_end + 1;
        }
      }

      if (fputs("<table class=\"nm-table\"", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }

      if (fputs(" style=\"", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }

      if (table_style.width.size > 0) {
        if (fputs("width:", out) < 0 || !print_html_escaped(out, &table_style.width) ||
            fputs(";", out) < 0) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }
      if (table_style.bgcolor.size > 0) {
        if (fputs("background-color:", out) < 0 || !print_html_escaped(out, &table_style.bgcolor) ||
            fputs(";", out) < 0) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }
      if (table_style.bordercolor.size > 0) {
        if (fputs("border:2px solid ", out) < 0 || !print_html_escaped(out, &table_style.bordercolor) ||
            fputs(";", out) < 0) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }
      if (table_style.align.size > 0) {
        if (table_style.align.size == 6 && memcmp(table_style.align.ptr, "center", 6) == 0) {
          if (fputs("left:auto;margin-right:auto;", out) < 0) {
            strbuf_free(&pending_row);
            strbuf_free(&tablebuf);
            table_context_free(&table_style);
            return 0;
          }
        } else if (table_style.align.size == 5 && memcmp(table_style.align.ptr, "right", 5) == 0) {
          if (fputs("left:auto;", out) < 0) {
            strbuf_free(&pending_row);
            strbuf_free(&tablebuf);
            table_context_free(&table_style);
            return 0;
          }
        }
      }

      if (fputs("\"><tbody>\n", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }

      while (line_start <= tablebuf.size) {
        bufsize_t line_end = strbuf_strchr(&tablebuf, '\n', line_start);
        if (line_end < 0) {
          line_end = tablebuf.size;
        }

        if (line_end > line_start) {
          const unsigned char *line_ptr = tablebuf.ptr + line_start;
          bufsize_t line_len = line_end - line_start;

          if (is_row_start_line(line_ptr, line_len)) {
            if (pending_row.size > 0 && table_row_ends_with_separator(&pending_row)) {
              if (!render_table_row(out, &pending_row, row_index, &table_style)) {
                strbuf_free(&pending_row);
                strbuf_free(&tablebuf);
                table_context_free(&table_style);
                return 0;
              }
              row_index++;
              strbuf_clear(&pending_row);
            }
            if (pending_row.size > 0) {
              strbuf_putc(&pending_row, '\n');
              strbuf_put(&pending_row, line_ptr, line_len);
            } else {
              strbuf_set(&pending_row, line_ptr, line_len);
            }
          } else if (pending_row.size > 0) {
            strbuf_putc(&pending_row, '\n');
            strbuf_put(&pending_row, line_ptr, line_len);
          }
        }

        if (line_end >= tablebuf.size) {
          break;
        }
        line_start = line_end + 1;
      }

      if (pending_row.size > 0) {
        if (!render_table_row(out, &pending_row, row_index, &table_style)) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }

      strbuf_free(&pending_row);
      strbuf_free(&tablebuf);
      table_context_free(&table_style);
      if (fputs("</tbody></table>\n", out) < 0) {
        return 0;
      }
      return 1;
    }
    case NAMUMARK_NODE_WIKI_BLOCK: {
      strbuf wikibuf;
      strbuf_init(&wikibuf, node->content.size + 1);
      strbuf_set(&wikibuf, node->content.ptr, node->content.size);

      namumark_parser *sub = parser_new();
      if (sub == NULL) {
        strbuf_free(&wikibuf);
        return 0;
      }

      parser_feed(sub, wikibuf.ptr, (size_t)wikibuf.size);
      namumark_node *subdoc = parser_finish(sub);
      parser_free(sub);
      strbuf_free(&wikibuf);

      if (subdoc == NULL) {
        return 0;
      }

      if (fputs("<div class=\"nm-wiki-block\"", out) < 0) {
        namumark_node_free(subdoc);
        return 0;
      }
      if (node->args.size > 0) {
        if (fputs(" style=\"", out) < 0 || !print_html_escaped(out, &node->args) ||
            fputs("\"", out) < 0) {
          namumark_node_free(subdoc);
          return 0;
        }
      }
      if (fputs(">\n", out) < 0) {
        namumark_node_free(subdoc);
        return 0;
      }

      if (!render_block_children(out, subdoc)) {
        namumark_node_free(subdoc);
        return 0;
      }

      namumark_node_free(subdoc);
      if (fputs("</div>\n", out) < 0) {
        return 0;
      }
      return 1;
    }
    case NAMUMARK_NODE_PREFORMATTED:
      if (fputs("<pre><code>", out) < 0 || !print_html_escaped(out, &node->content) ||
          fputs("</code></pre>\n", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_CATEGORY:
      if (fputs("<!-- category: ", out) < 0 || !print_html_escaped(out, &node->target) ||
          fputs(" -->\n", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_FOOTNOTE_DEFINITION:
      if (fputs("<div class=\"nm-footnote\" data-label=\"", out) < 0 ||
          !print_html_escaped(out, &node->label) || fputs("\">", out) < 0 ||
          !render_inline_children(out, node) || fputs("</div>\n", out) < 0) {
        return 0;
      }
      return 1;
    case NAMUMARK_NODE_TEXT:
      if (fputs("<p>", out) < 0 || !render_inline_children(out, node) || fputs("</p>\n", out) < 0) {
        return 0;
      }
      return 1;
    default:
      if (fputs("<div>", out) < 0 || !render_inline_children(out, node) || fputs("</div>\n", out) < 0) {
        return 0;
      }
      return 1;
  }
}

static int render_block_children(FILE *out, const namumark_node *parent) {
  const namumark_node *child = parent->first_child;
  while (child != NULL) {
    if (!render_block_node(out, child)) {
      return 0;
    }
    /* If LIST, skip following LIST siblings (already consumed inside render_block_node) */
    if (child->type == NAMUMARK_NODE_LIST) {
      while (child->next != NULL && is_rendered_list_sequence_node(child->next)) {
        child = child->next;
      }
    }
    child = child->next;
  }
  return 1;
}

int print_document_html(const namumark_node *document, FILE *out) {
  if (document == NULL || out == NULL) {
    return 0;
  }

  if (fputs("<article class=\"namumark\">\n", out) < 0) {
    return 0;
  }

  if (!render_block_children(out, document)) {
    return 0;
  }

  if (fputs("</article>\n", out) < 0) {
    return 0;
  }
  return 1;
}

int print_document(const namumark_node *document, FILE *out) {
  return print_document_ast(document, out);
}

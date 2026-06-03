#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "inlines.h"
#include "node.h"

static inline bool S_is_line_end_char(unsigned char c) {
  return c == '\n' || c == '\r';
}

bool is_line_end_char(unsigned char c) {
  return S_is_line_end_char(c);
}

static bool starts_with_at(const strbuf *source, bufsize_t pos, const char *token) {
  bufsize_t token_len = (bufsize_t)strlen(token);
  if (source == NULL || token == NULL || token_len == 0) {
    return false;
  }
  if (pos < 0 || pos + token_len > source->size) {
    return false;
  }
  return memcmp(source->ptr + pos, token, token_len) == 0;
}

static int is_ascii_alpha(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_ascii_hex(unsigned char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_valid_color_spec(const unsigned char *value, bufsize_t len) {
  if (value == NULL || len <= 0) {
    return 0;
  }

  const unsigned char *primary = value;
  bufsize_t primary_len = len;
  for (bufsize_t i = 0; i < len; i++) {
    if (value[i] == ',') {
      primary_len = i;
      break;
    }
  }

  if (primary_len <= 0) {
    return 0;
  }

  if (primary[0] == '#') {
    primary++;
    primary_len--;
  }

  if (primary_len == 3 || primary_len == 6) {
    for (bufsize_t i = 0; i < primary_len; i++) {
      if (!is_ascii_hex(primary[i])) {
        goto check_alias;
      }
    }
    return 1;
  }

check_alias:
  if (!is_ascii_alpha(primary[0])) {
    return 0;
  }
  for (bufsize_t i = 1; i < primary_len; i++) {
    if (!is_ascii_alpha(primary[i]) && primary[i] != '-' && primary[i] != '_') {
      return 0;
    }
  }
  return 1;
}

static bufsize_t find_token(const strbuf *source, const char *token, bufsize_t from) {
  bufsize_t token_len = (bufsize_t)strlen(token);
  if (token_len == 0 || from < 0 || from >= source->size) {
    return -1;
  }

  for (bufsize_t i = from; i + token_len <= source->size; i++) {
    if (memcmp(source->ptr + i, token, token_len) == 0) {
      return i;
    }
  }
  return -1;
}

static bufsize_t find_link_close(const strbuf *source, bufsize_t open_pos) {
  if (source == NULL || open_pos < 0 || open_pos + 2 > source->size) {
    return -1;
  }

  int depth = 1;
  for (bufsize_t i = open_pos + 2; i + 1 < source->size; i++) {
    if (source->ptr[i] == '\\') {
      if (i + 1 < source->size) {
        i++;
      }
      continue;
    }

    if (source->ptr[i] == '[' && source->ptr[i + 1] == '[') {
      depth++;
      i++;
      continue;
    }

    if (source->ptr[i] == ']' && source->ptr[i + 1] == ']') {
      depth--;
      if (depth == 0) {
        return i;
      }
      i++;
      continue;
    }
  }

  return -1;
}

static bufsize_t find_advanced_close(const strbuf *source, bufsize_t open_pos) {
  if (source == NULL || open_pos < 0 || open_pos + 3 > source->size) {
    return -1;
  }

  int depth = 1;
  bufsize_t i = open_pos + 3;
  while (i + 2 < source->size) {
    if (memcmp(source->ptr + i, "{{{", 3) == 0) {
      depth++;
      i += 3;
      continue;
    }
    if (memcmp(source->ptr + i, "}}}", 3) == 0) {
      depth--;
      if (depth == 0) {
        return i;
      }
      i += 3;
      continue;
    }
    i++;
  }

  return -1;
}

static void append_text_segment(const strbuf *source, bufsize_t start, bufsize_t len,
                                namumark_node *parent, int line_number) {
  if (source == NULL || parent == NULL || len <= 0) {
    return;
  }

  namumark_node *text = namumark_node_new(NAMUMARK_NODE_TEXT, line_number, (int)start + 1);
  if (text == NULL) {
    return;
  }

  strbuf_set(&text->content, source->ptr + start, len);
  text->end_line = line_number;
  text->end_column = (int)(start + len);
  text->flags = (namumark_node_internal_flags)0;
  namumark_node_append_child(parent, text);
}

static void append_node_from_range(namumark_node_type node_type, const strbuf *source,
                                   bufsize_t start, bufsize_t len,
                                   namumark_node *parent, int line_number) {
  namumark_node *node = namumark_node_new(node_type, line_number, (int)start + 1);
  if (node == NULL) {
    return;
  }

  strbuf_set(&node->content, source->ptr + start, len);
  node->end_line = line_number;
  node->end_column = (int)(start + len);
  node->flags = (namumark_node_internal_flags)0;
  namumark_node_append_child(parent, node);
}

static void parse_last_child_inlines(namumark_node *parent, int line_number) {
  if (parent == NULL || parent->last_child == NULL) {
    return;
  }

  namumark_node *node = parent->last_child;
  parse_inlines(&node->content, node, line_number);
}

static void parse_link_target(namumark_node *node) {
  if (node == NULL || node->content.size <= 0) {
    return;
  }

  bufsize_t sep = strbuf_strchr(&node->content, '|', 0);
  if (sep < 0) {
    strbuf_set(&node->target, node->content.ptr, node->content.size);
  } else {
    strbuf_set(&node->target, node->content.ptr, sep);
    if (sep + 1 < node->content.size) {
      strbuf_set(&node->args, node->content.ptr + sep + 1, node->content.size - sep - 1);
    }
  }

  if (node->target.size > 0) {
    if (node->target.ptr[0] == '#') {
      node->link_type = NAMUMARK_LINK_ANCHOR;
    } else if ((node->target.size >= 8 && memcmp(node->target.ptr, "https://", 8) == 0) ||
               (node->target.size >= 7 && memcmp(node->target.ptr, "http://", 7) == 0) ||
               (node->target.size >= 6 && memcmp(node->target.ptr, "ftp://", 6) == 0)) {
      node->link_type = NAMUMARK_LINK_EXTERNAL;
    } else if (node->target.size >= 7 && memcmp(node->target.ptr, "파일:", 7) == 0) {
      node->link_type = NAMUMARK_LINK_FILE;
    } else if (node->target.size >= 7 && memcmp(node->target.ptr, "분류:", 7) == 0) {
      node->link_type = NAMUMARK_LINK_CATEGORY;
    } else if (node->target.ptr[0] == '/' ||
               (node->target.size >= 3 && node->target.ptr[0] == '.' && node->target.ptr[1] == '.' && node->target.ptr[2] == '/')) {
      node->link_type = NAMUMARK_LINK_RELATIVE;
    } else {
      node->link_type = NAMUMARK_LINK_INTERNAL;
    }
  }
}

static void parse_macro_content(namumark_node *node) {
  if (node == NULL || node->content.size <= 0) {
    return;
  }

  bufsize_t open = strbuf_strchr(&node->content, '(', 0);
  if (open < 0) {
    strbuf_set(&node->target, node->content.ptr, node->content.size);
  } else {
    strbuf_set(&node->target, node->content.ptr, open);

    if (node->content.size >= open + 2 && node->content.ptr[node->content.size - 1] == ')') {
      strbuf_set(&node->args, node->content.ptr + open + 1, node->content.size - open - 2);
    }
  }

  if (node->target.size == 7 && memcmp(node->target.ptr, "include", 7) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_INCLUDE;
  } else if (node->target.size == 3 && memcmp(node->target.ptr, "age", 3) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_AGE;
  } else if (node->target.size == 4 && memcmp(node->target.ptr, "date", 4) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_DATE;
  } else if (node->target.size == 4 && memcmp(node->target.ptr, "dday", 4) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_DDAY;
  } else if ((node->target.size == 6 && memcmp(node->target.ptr, "목차", 6) == 0) ||
             (node->target.size == 14 && memcmp(node->target.ptr, "tableofcontents", 14) == 0)) {
    node->macro_type = NAMUMARK_NODE_MACRO_TOC;
  } else if ((node->target.size == 6 && memcmp(node->target.ptr, "각주", 6) == 0) ||
             (node->target.size == 8 && memcmp(node->target.ptr, "footnote", 8) == 0)) {
    node->macro_type = NAMUMARK_NODE_MACRO_FOOTNOTE;
  } else if (node->target.size == 2 && memcmp(node->target.ptr, "br", 2) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_BREAKLINE;
  } else if (node->target.size == 8 && memcmp(node->target.ptr, "clearfix", 8) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_CLEARFIX;
  } else if (node->target.size == 9 && memcmp(node->target.ptr, "pagecount", 9) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_PAGECOUNT;
  } else if (node->target.size == 4 && memcmp(node->target.ptr, "ruby", 4) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_RUBY;
  }
}

static void parse_advanced_content(namumark_node *node) {
  if (node == NULL || node->content.size == 0) {
    return;
  }

  if (node->content.size >= 2 && node->content.ptr[0] == '#' && node->content.ptr[1] == '!') {
    if (node->content.size >= 10 && memcmp(node->content.ptr, "#!folding", 9) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_FOLDING;
      return;
    }
    if (node->content.size >= 7 && memcmp(node->content.ptr, "#!wiki", 6) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_WIKI;

      bufsize_t i = 6;
      while (i < node->content.size && (node->content.ptr[i] == ' ' || node->content.ptr[i] == '\t')) {
        i++;
      }

      if (i + 6 <= node->content.size && memcmp(node->content.ptr + i, "style=", 6) == 0) {
        i += 6;
        if (i < node->content.size) {
          if (node->content.ptr[i] == '"') {
            bufsize_t q = i + 1;
            while (q < node->content.size && node->content.ptr[q] != '"') {
              q++;
            }
            if (q > i + 1 && q < node->content.size) {
              strbuf_set(&node->args, node->content.ptr + i + 1, q - (i + 1));
            }
          } else {
            bufsize_t end = i;
            while (end < node->content.size && node->content.ptr[end] != ' ' &&
                   node->content.ptr[end] != '\t' && node->content.ptr[end] != '\n' &&
                   node->content.ptr[end] != '\r') {
              end++;
            }
            if (end > i) {
              strbuf_set(&node->args, node->content.ptr + i, end - i);
            }
          }
        }
      }

      return;
    }
    if (node->content.size >= 8 && memcmp(node->content.ptr, "#!style", 7) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_STYLE;
      return;
    }
    if (node->content.size >= 9 && memcmp(node->content.ptr, "#!syntax", 8) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_SYNTAX;
      return;
    }
    if (node->content.size >= 5 && memcmp(node->content.ptr, "#!if", 4) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_IF;
      return;
    }
    if (node->content.size >= 7 && memcmp(node->content.ptr, "#!html", 6) == 0) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_HTML;
      return;
    }
  }

  if (node->content.size > 0 && (node->content.ptr[0] == '+' || node->content.ptr[0] == '-')) {
    node->advanced_type = NAMUMARK_NODE_ADVANCED_SIZING;
    return;
  }

  if (node->content.size > 0 && node->content.ptr[0] == '#') {
    bufsize_t token_end = 1;
    while (token_end < node->content.size && node->content.ptr[token_end] != ' ' &&
           node->content.ptr[token_end] != '\t' && node->content.ptr[token_end] != '\n' &&
           node->content.ptr[token_end] != '\r') {
      token_end++;
    }

    if (is_valid_color_spec(node->content.ptr, token_end)) {
      node->advanced_type = NAMUMARK_NODE_ADVANCED_COLOR;
      return;
    }
  }

  node->advanced_type = NAMUMARK_NODE_ADVANCED_LITERAL;
}

static bool find_macro_close(const strbuf *source, bufsize_t open_bracket, bufsize_t *close_out) {
  if (source == NULL || close_out == NULL) {
    return false;
  }

  if (open_bracket + 1 >= source->size) {
    return false;
  }

  bufsize_t i = open_bracket + 1;
  while (i < source->size && source->ptr[i] != ']' && source->ptr[i] != '(' && source->ptr[i] != ' ' && source->ptr[i] != '\t') {
    i++;
  }

  if (i >= source->size) {
    return false;
  }

  if (source->ptr[i] == ']') {
    *close_out = i;
    return true;
  }

  if (source->ptr[i] != '(') {
    return false;
  }

  int depth = 1;
  i++;
  while (i < source->size) {
    if (source->ptr[i] == '(') {
      depth++;
    } else if (source->ptr[i] == ')') {
      depth--;
      if (depth == 0) {
        if (i + 1 < source->size && source->ptr[i + 1] == ']') {
          *close_out = i + 1;
          return true;
        }
        return false;
      }
    }
    i++;
  }

  return false;
}

void parse_inlines(const strbuf *source, namumark_node *parent, int line_number) {
  if (source == NULL || parent == NULL) {
    return;
  }

  bufsize_t i = 0;
  bufsize_t plain_start = 0;

  while (i < source->size) {
    if (starts_with_at(source, i, "{{{")) {
      bufsize_t close = find_advanced_close(source, i);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_ADVANCED, source, i + 3, close - (i + 3), parent,
                               line_number);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_ADVANCED) {
          parse_advanced_content(parent->last_child);
        }
        i = close + 3;
        plain_start = i;
        continue;
      }
    }

    if (i == 0 && starts_with_at(source, i, "##")) {
      append_text_segment(source, plain_start, i - plain_start, parent, line_number);
      append_node_from_range(NAMUMARK_NODE_COMMENT, source, i + 2, source->size - (i + 2),
                             parent, line_number);
      if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_COMMENT &&
          source->size >= i + 3 && source->ptr[i + 2] == '@') {
        parent->last_child->fixed_comment = 1;
        strbuf_set(&parent->last_child->content, source->ptr + i + 3, source->size - (i + 3));
      }
      return;
    }

    if (starts_with_at(source, i, "[[")) {
      bufsize_t close = find_link_close(source, i);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_LINK, source, i + 2, close - (i + 2), parent,
                               line_number);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_LINK) {
          parse_link_target(parent->last_child);
        }
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "[*")) {
      bufsize_t close = find_token(source, "]", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_FOOTNOTE_REFERENCE, source, i + 2,
                               close - (i + 2), parent, line_number);
        i = close + 1;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "<math>")) {
      bufsize_t close = find_token(source, "</math>", i + 6);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_MACRO, source, i, (close + 7) - i, parent,
                               line_number);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_MACRO) {
          strbuf_set(&parent->last_child->target, (const unsigned char *)"math", 4);
          strbuf_set(&parent->last_child->args, source->ptr + i + 6, close - (i + 6));
          parent->last_child->macro_type = NAMUMARK_NODE_MACRO_NONE;
        }
        i = close + 7;
        plain_start = i;
        continue;
      }
    }

    if (source->ptr[i] == '[') {
      bufsize_t close = -1;
      if (find_macro_close(source, i, &close)) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_MACRO, source, i + 1, close - (i + 1), parent,
                               line_number);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_MACRO) {
          parse_macro_content(parent->last_child);
        }
        i = close + 1;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "'''")) {
      bufsize_t close = find_token(source, "'''", i + 3);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_BOLD, source, i + 3, close - (i + 3), parent,
                               line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 3;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "''")) {
      bufsize_t close = find_token(source, "''", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_ITALIC, source, i + 2, close - (i + 2), parent,
                               line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "__")) {
      bufsize_t close = find_token(source, "__", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_UNDERLINE, source, i + 2, close - (i + 2), parent,
                               line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "~~")) {
      bufsize_t close = find_token(source, "~~", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_STRIKETHROUGH, source, i + 2, close - (i + 2),
                               parent, line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "--")) {
      bufsize_t close = find_token(source, "--", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_STRIKETHROUGH, source, i + 2, close - (i + 2),
                               parent, line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, "^^")) {
      bufsize_t close = find_token(source, "^^", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_SUPERSCRIPT, source, i + 2, close - (i + 2),
                               parent, line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    if (starts_with_at(source, i, ",,")) {
      bufsize_t close = find_token(source, ",,", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number);
        append_node_from_range(NAMUMARK_NODE_SUBSCRIPT, source, i + 2, close - (i + 2), parent,
                               line_number);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    i++;
  }

  append_text_segment(source, plain_start, source->size - plain_start, parent, line_number);
}

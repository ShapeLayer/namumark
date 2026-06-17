/**
 * @file inlines.c
 * @brief Inline syntax parser for text already grouped into block nodes.
 *
 * The parser is permissive by design.  Unknown bracket constructs are kept as
 * macro nodes and later rendered as literal text, preserving examples such as
 * [v] while still allowing known forms like [br], [ruby(...)], and footnotes to
 * receive specialized output.
 */
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "inlines.h"
#include "node.h"

static inline bool S_is_line_end_char(unsigned char c) {
  /* Keep the public wrapper tiny while tests can exercise the same predicate. */
  return c == '\n' || c == '\r';
}

bool is_line_end_char(unsigned char c) {
  return S_is_line_end_char(c);
}

static bool starts_with_at(const strbuf *source, bufsize_t pos, const char *token) {
  /* Byte-level prefix check used by every delimiter branch. */
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
  /* Color aliases and keywords are ASCII grammar tokens. */
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_ascii_hex(unsigned char c) {
  /* Hex colors are parsed byte-wise before UTF-8 text matters. */
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_valid_color_spec(const unsigned char *value, bufsize_t len) {
  /*
   * Color advanced syntax accepts both #rgb/#rrggbb and named aliases.  Only
   * the primary color is validated here; a comma-separated dark-mode color is
   * carried through to the renderer as data-dark-style.
   */
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

static int attr_name_matches(const unsigned char *name, bufsize_t name_len, const char *attr) {
  /* {{{#!wiki}}} attribute names are ASCII and case-sensitive. */
  bufsize_t attr_len = (bufsize_t)strlen(attr);
  return name_len == attr_len && memcmp(name, attr, (size_t)attr_len) == 0;
}

static void extract_wiki_attr(const unsigned char *text, bufsize_t len, const char *attr,
                              bufsize_t *value_start, bufsize_t *value_len) {
  /*
   * {{{#!wiki}}} attributes are parsed from the opening line only.  Values may
   * be quoted or unquoted, but they never span newlines; the rest of the block is
   * content and must not be scanned as attributes.
   */
  *value_start = 0;
  *value_len = 0;

  bufsize_t i = 6;
  while (i < len && (text[i] == ' ' || text[i] == '\t')) {
    i++;
  }

  while (i < len && text[i] != '\n' && text[i] != '\r') {
    while (i < len && (text[i] == ' ' || text[i] == '\t')) {
      i++;
    }
    bufsize_t name_start = i;
    while (i < len && ((text[i] >= 'a' && text[i] <= 'z') ||
                       (text[i] >= 'A' && text[i] <= 'Z') ||
                       (text[i] >= '0' && text[i] <= '9') || text[i] == '-')) {
      i++;
    }
    bufsize_t name_len = i - name_start;
    while (i < len && (text[i] == ' ' || text[i] == '\t')) {
      i++;
    }
    if (name_len == 0 || i >= len || text[i] != '=') {
      return;
    }
    i++;
    while (i < len && (text[i] == ' ' || text[i] == '\t')) {
      i++;
    }

    bufsize_t val_start = i;
    bufsize_t val_len = 0;
    if (i < len && text[i] == '"') {
      val_start = i + 1;
      i++;
      while (i < len && text[i] != '"') {
        i++;
      }
      val_len = i - val_start;
      if (i < len && text[i] == '"') {
        i++;
      }
    } else {
      while (i < len && text[i] != ' ' && text[i] != '\t' && text[i] != '\n' &&
             text[i] != '\r') {
        i++;
      }
      val_len = i - val_start;
    }

    if (attr_name_matches(text + name_start, name_len, attr)) {
      *value_start = val_start;
      *value_len = val_len;
      return;
    }
  }
}

static bufsize_t find_token(const strbuf *source, const char *token, bufsize_t from) {
  /* Generic forward search for non-nesting inline delimiters. */
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
  /*
   * Links can contain nested links in labels, and escaped brackets must not end
   * the outer link.  The depth counter keeps constructs like [[file|[[label]]]]
   * as one link node instead of two broken anchors.
   */
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

static bufsize_t find_advanced_close(const strbuf *source, bufsize_t open_pos);
static bool find_macro_close(const strbuf *source, bufsize_t open_bracket, bufsize_t *close_out);

static bufsize_t find_footnote_close(const strbuf *source, bufsize_t open_pos) {
  /*
   * Footnote bodies are inline mini-documents.  Their closing ']' must ignore
   * nested links, macros, and triple-brace advanced sections so references like
   * [* [[Page|label]] text] do not stop at the link's bracket.
   */
  if (source == NULL || open_pos < 0 || open_pos + 2 > source->size) {
    return -1;
  }

  for (bufsize_t i = open_pos + 2; i < source->size; i++) {
    if (source->ptr[i] == '\\') {
      if (i + 1 < source->size) {
        i++;
      }
      continue;
    }

    if (i + 2 <= source->size && memcmp(source->ptr + i, "[[", 2) == 0) {
      bufsize_t close = find_link_close(source, i);
      if (close >= 0) {
        i = close + 1;
        continue;
      }
    }

    if (i + 3 <= source->size && memcmp(source->ptr + i, "{{{", 3) == 0) {
      bufsize_t close = find_advanced_close(source, i);
      if (close >= 0) {
        i = close + 2;
        continue;
      }
    }

    if (source->ptr[i] == '[') {
      bufsize_t close = -1;
      if (find_macro_close(source, i, &close)) {
        i = close;
        continue;
      }
    }

    if (source->ptr[i] == ']') {
      return i;
    }
  }

  return -1;
}

static bufsize_t find_advanced_close(const strbuf *source, bufsize_t open_pos) {
  /*
   * Advanced {{{...}}} syntax nests by brace count.  This is used for literal
   * examples containing another {{{#!wiki}}}; stopping at the first close would
   * leak braces or split examples in documentation tables.
   */
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
                                namumark_node *parent, int line_number, int base_column) {
  /* Preserve plain text as explicit nodes so AST JSON can show parser splits. */
  if (source == NULL || parent == NULL || len <= 0) {
    return;
  }

  /* Absolute 1-based line column of the first byte; half-open end. */
  int start_column = base_column + (int)start;
  namumark_node *text = namumark_node_new(NAMUMARK_NODE_TEXT, line_number, start_column);
  if (text == NULL) {
    return;
  }

  strbuf_set(&text->content, source->ptr + start, len);
  text->end_line = line_number;
  text->end_column = start_column + (int)len;
  text->flags = (namumark_node_internal_flags)0;
  namumark_node_append_child(parent, text);
}

static void append_node_from_range(namumark_node_type node_type, const strbuf *source,
                                   bufsize_t start, bufsize_t len,
                                   namumark_node *parent, int line_number, int base_column) {
  /* Helper for syntax spans whose raw body still needs subtype parsing. */
  int start_column = base_column + (int)start;
  namumark_node *node = namumark_node_new(node_type, line_number, start_column);
  if (node == NULL) {
    return;
  }

  strbuf_set(&node->content, source->ptr + start, len);
  node->end_line = line_number;
  node->end_column = start_column + (int)len;
  node->flags = (namumark_node_internal_flags)0;
  namumark_node_append_child(parent, node);
}

static void parse_last_child_inlines(namumark_node *parent, int line_number) {
  /* Emphasis nodes can contain nested inline syntax. */
  if (parent == NULL || parent->last_child == NULL) {
    return;
  }

  namumark_node *node = parent->last_child;
  /*
   * The child's content was sliced starting at node->start_column (absolute,
   * 1-based), so nested inline spans must continue from that same base to stay
   * in absolute line coordinates instead of resetting to 1.
   */
  parse_inlines(&node->content, node, line_number, node->start_column);
}

static void unescape_link_text(strbuf *out, const unsigned char *text, bufsize_t len) {
  /*
   * Link targets use backslash as an escape for otherwise structural characters.
   * The renderer works with normalized targets so file names such as
   * 파일:\#name.jpg and document links such as S\#ARP render as their visible
   * document names.
   */
  strbuf_clear(out);
  for (bufsize_t i = 0; i < len; i++) {
    if (text[i] == '\\' && i + 1 < len) {
      if (text[i + 1] == '\\') {
        strbuf_putc(out, '\\');
        i++;
      } else {
        strbuf_putc(out, text[i + 1]);
        i++;
      }
    } else {
      strbuf_putc(out, text[i]);
    }
  }
}

static void normalize_link_target(strbuf *target, strbuf *display) {
  /*
   * NamuMark has several target spelling conveniences: trailing '#' can escape
   * a literal hash, '문서:' is an explicit namespace prefix, and section suffixes
   * such as #s-2 should remain in href while disappearing from the default label.
   */
  if (target == NULL || target->size == 0) {
    return;
  }

  strbuf normalized;
  strbuf_init(&normalized, target->size + 1);
  unescape_link_text(&normalized, target->ptr, target->size);

  if (normalized.size >= 7 && memcmp(normalized.ptr, "문서:", 7) == 0) {
    strbuf_drop(&normalized, 7);
  }

  int stripped_trailing_hash = 0;
  if (normalized.size >= 2 && normalized.ptr[normalized.size - 1] == '#' &&
      normalized.ptr[normalized.size - 2] != '#') {
    strbuf_truncate(&normalized, normalized.size - 1);
    stripped_trailing_hash = 1;
  } else if (normalized.size >= 2 && normalized.ptr[normalized.size - 1] == '#' &&
             normalized.ptr[normalized.size - 2] == '#') {
    strbuf_truncate(&normalized, normalized.size - 1);
  }

  if (display != NULL && display->size == 0) {
    strbuf_set(display, normalized.ptr, normalized.size);
    if (!stripped_trailing_hash && display->size > 3) {
      for (bufsize_t i = 1; i + 2 < display->size; i++) {
        if (display->ptr[i] == '#' && display->ptr[i + 1] == 's' && display->ptr[i + 2] == '-') {
          strbuf_truncate(display, i);
          break;
        }
      }
    }
  }

  strbuf_set(target, normalized.ptr, normalized.size);
  strbuf_free(&normalized);
}

static void parse_link_target(namumark_node *node) {
  /* Split [[target|label]], normalize target spelling, then classify link kind. */
  if (node == NULL || node->content.size <= 0) {
    return;
  }

  bufsize_t sep = strbuf_strchr(&node->content, '|', 0);
  if (sep < 0) {
    strbuf_set(&node->target, node->content.ptr, node->content.size);
  } else {
    strbuf_set(&node->target, node->content.ptr, sep);
    if (sep + 1 < node->content.size) {
      unescape_link_text(&node->args, node->content.ptr + sep + 1, node->content.size - sep - 1);
    }
  }

  normalize_link_target(&node->target, &node->args);

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
  /*
   * Macro target and args are split once here.  Renderers decide which targets
   * deserve special HTML.  Unknown targets intentionally keep macro_type NONE so
   * they can fall back to visible bracket text instead of silently disappearing.
   */
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
  } else if (node->target.size == 7 && memcmp(node->target.ptr, "youtube", 7) == 0) {
    node->macro_type = NAMUMARK_NODE_MACRO_YOUTUBE;
  }
}

static void parse_advanced_content(namumark_node *node) {
  /*
   * Advanced nodes all share the same triple-brace delimiters, so the first
   * token inside the braces selects the subtype.  {{{#!wiki}}} also extracts
   * attributes here because both block and inline wiki renderers need the same
   * style/class/onClick/tag metadata.
   */
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

      bufsize_t value_start = 0;
      bufsize_t value_len = 0;
      extract_wiki_attr(node->content.ptr, node->content.size, "style", &value_start, &value_len);
      if (value_len > 0) {
        strbuf_set(&node->args, node->content.ptr + value_start, value_len);
      }
      extract_wiki_attr(node->content.ptr, node->content.size, "class", &value_start, &value_len);
      if (value_len > 0) {
        strbuf_set(&node->label, node->content.ptr + value_start, value_len);
      }
      extract_wiki_attr(node->content.ptr, node->content.size, "dark-style", &value_start, &value_len);
      if (value_len > 0) {
        strbuf_set(&node->target, node->content.ptr + value_start, value_len);
      }
      extract_wiki_attr(node->content.ptr, node->content.size, "onclick", &value_start, &value_len);
      if (value_len > 0) {
        strbuf_set(&node->onclick, node->content.ptr + value_start, value_len);
      }
      extract_wiki_attr(node->content.ptr, node->content.size, "tag", &value_start, &value_len);
      if (value_len > 0) {
        strbuf_set(&node->tag, node->content.ptr + value_start, value_len);
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
  /*
   * Macro recognition is intentionally broad; unknown macros are rendered back
   * as literal [text].  This allows known forms like [ruby(...)] and [youtube]
   * to be recognized without breaking prose examples such as [v] or [A].
   */
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

void parse_inlines(const strbuf *source, namumark_node *parent, int line_number, int base_column) {
  /*
   * The scan is single-pass and appends plain text before each recognized span.
   * Ordering matters: footnotes and links must be tested before generic macros,
   * and escapes must be consumed before any delimiter checks.
   *
   * base_column is the absolute 1-based line column of source[0]; every emitted
   * span is reported in absolute line coordinates via base_column + byte offset.
   */
  if (source == NULL || parent == NULL) {
    return;
  }

  bufsize_t i = 0;
  bufsize_t plain_start = 0;

  while (i < source->size) {
    /* Backslash escapes punctuation before any delimiter can claim it. */
    if (source->ptr[i] == '\\' && i + 1 < source->size && ispunct((unsigned char)source->ptr[i + 1])) {
      append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
      append_text_segment(source, i + 1, 1, parent, line_number, base_column);
      i += 2;
      plain_start = i;
      continue;
    }

    /* Triple braces cover inline literals, colors/sizing, and inline #!wiki blocks. */
    if (starts_with_at(source, i, "{{{")) {
      bufsize_t close = find_advanced_close(source, i);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_ADVANCED, source, i + 3, close - (i + 3), parent,
                               line_number, base_column);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_ADVANCED) {
          parse_advanced_content(parent->last_child);
        }
        i = close + 3;
        plain_start = i;
        continue;
      }
    }

    /* A line-start ## is a real comment; indented ## is handled as visible text. */
    if (i == 0 && starts_with_at(source, i, "##")) {
      append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
      append_node_from_range(NAMUMARK_NODE_COMMENT, source, i + 2, source->size - (i + 2),
                             parent, line_number, base_column);
      if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_COMMENT &&
          source->size >= i + 3 && source->ptr[i + 2] == '@') {
        parent->last_child->fixed_comment = 1;
        strbuf_set(&parent->last_child->content, source->ptr + i + 3, source->size - (i + 3));
      }
      return;
    }

    /* Links are checked before macros because both begin with '['. */
    if (starts_with_at(source, i, "[[")) {
      bufsize_t close = find_link_close(source, i);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_LINK, source, i + 2, close - (i + 2), parent,
                               line_number, base_column);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_LINK) {
          parse_link_target(parent->last_child);
        }
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Inline footnotes may contain nested links/macros, so they need a custom close scan. */
    if (starts_with_at(source, i, "[*")) {
      bufsize_t close = find_footnote_close(source, i);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_FOOTNOTE_REFERENCE, source, i + 2,
                               close - (i + 2), parent, line_number, base_column);
        i = close + 1;
        plain_start = i;
        continue;
      }
    }

    /* Math-like raw spans use XML-looking delimiters rather than bracket macros. */
    if (starts_with_at(source, i, "<math>")) {
      bufsize_t close = find_token(source, "</math>", i + 6);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_MACRO, source, i, (close + 7) - i, parent,
                               line_number, base_column);
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

    /* Generic bracket macros are parsed after all more specific bracket forms. */
    if (source->ptr[i] == '[') {
      bufsize_t close = -1;
      if (find_macro_close(source, i, &close)) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_MACRO, source, i + 1, close - (i + 1), parent,
                               line_number, base_column);
        if (parent->last_child != NULL && parent->last_child->type == NAMUMARK_NODE_MACRO) {
          parse_macro_content(parent->last_child);
        }
        i = close + 1;
        plain_start = i;
        continue;
      }
    }

    /* Strong emphasis must be checked before italic because both start with ''. */
    if (starts_with_at(source, i, "'''")) {
      bufsize_t close = find_token(source, "'''", i + 3);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_BOLD, source, i + 3, close - (i + 3), parent,
                               line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 3;
        plain_start = i;
        continue;
      }
    }

    /* Italic text uses a double apostrophe pair. */
    if (starts_with_at(source, i, "''")) {
      bufsize_t close = find_token(source, "''", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_ITALIC, source, i + 2, close - (i + 2), parent,
                               line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Underline is parsed after emphasis so apostrophe delimiters keep priority. */
    if (starts_with_at(source, i, "__")) {
      bufsize_t close = find_token(source, "__", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_UNDERLINE, source, i + 2, close - (i + 2), parent,
                               line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Both ~~text~~ and --text-- map to strikethrough. */
    if (starts_with_at(source, i, "~~")) {
      bufsize_t close = find_token(source, "~~", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_STRIKETHROUGH, source, i + 2, close - (i + 2),
                               parent, line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Hyphen strikethrough is checked after horizontal rules have already been handled by blocks.c. */
    if (starts_with_at(source, i, "--")) {
      bufsize_t close = find_token(source, "--", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_STRIKETHROUGH, source, i + 2, close - (i + 2),
                               parent, line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Superscript and subscript are the final paired inline delimiters. */
    if (starts_with_at(source, i, "^^")) {
      bufsize_t close = find_token(source, "^^", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_SUPERSCRIPT, source, i + 2, close - (i + 2),
                               parent, line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    /* Subscript uses doubled commas; unmatched commas remain plain text. */
    if (starts_with_at(source, i, ",,")) {
      bufsize_t close = find_token(source, ",,", i + 2);
      if (close >= 0) {
        append_text_segment(source, plain_start, i - plain_start, parent, line_number, base_column);
        append_node_from_range(NAMUMARK_NODE_SUBSCRIPT, source, i + 2, close - (i + 2), parent,
                               line_number, base_column);
        parse_last_child_inlines(parent, line_number);
        i = close + 2;
        plain_start = i;
        continue;
      }
    }

    i++;
  }

  append_text_segment(source, plain_start, source->size - plain_start, parent, line_number, base_column);
}

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "blocks.h"
#include "inlines.h"
#include "node.h"
#include "parser.h"

/**
 * @brief Allocate a block node at a source position.
 *
 * This wrapper exists so block parsing code reads in terms of block concepts,
 * while node.c remains the single owner of allocation details.
 */
static namumark_node *make_block(namumark_node_type node_type, int start_line, int start_column) {
  return namumark_node_new(node_type, start_line, start_column);
}

/**
 * @brief Remove CR/LF bytes from the current physical line.
 *
 * parser.c normalizes chunk boundaries, but blocks.c owns grammar decisions and
 * should never see line-ending bytes as content.  The trailing NUL is restored
 * for debug convenience; parsing uses explicit lengths.
 */
static void trim_line_end(strbuf *line) {
  while (line->size > 0) {
    unsigned char c = line->ptr[line->size - 1];
    if (c != '\n' && c != '\r') {
      break;
    }
    line->size--;
  }
  if (line->asize > 0) {
    line->ptr[line->size] = '\0';
  }
}

static bufsize_t skip_spaces(const unsigned char *s, bufsize_t len, bufsize_t from) {
  /* Skip only literal spaces.  Tabs are meaningful in some NamuMark examples. */
  while (from < len && s[from] == ' ') {
    from++;
  }
  return from;
}

static void trim_edges(const unsigned char *s, bufsize_t *start, bufsize_t *end) {
  /* Used for metadata targets where surrounding whitespace is not significant. */
  while (*start < *end && isspace((unsigned char)s[*start])) {
    (*start)++;
  }
  while (*end > *start && isspace((unsigned char)s[*end - 1])) {
    (*end)--;
  }
}

static namumark_node *append_block_text(namumark_parser *parser, namumark_node_type type,
                                        const unsigned char *text, bufsize_t len) {
  /* Most block recognizers finish by appending a raw content node to root. */
  namumark_node *node = make_block(type, parser->line_number, 1);
  if (node == NULL) {
    return NULL;
  }

  if (len > 0 && text != NULL) {
    strbuf_set(&node->content, text, len);
  }

  node->end_line = parser->line_number;
  node->end_column = (int)len;
  node->flags = (namumark_node_internal_flags)0;
  namumark_node_append_child(parser->root, node);

  return node;
}

static void set_block_target(namumark_node *node, const unsigned char *text, bufsize_t len) {
  /* Redirect and link-like block nodes keep their semantic target separately. */
  if (node == NULL) {
    return;
  }
  strbuf_set(&node->target, text, len);
}

/**
 * @brief Parse heading fences and optional folded-heading markers.
 *
 * NamuMark headings require symmetric left/right '=' counts and spaces around
 * content.  The folded marker '#' must appear on both sides; accepting only one
 * side would make malformed headings disappear instead of rendering as text.
 */
static int parse_heading_level(const unsigned char *line, bufsize_t len,
                               bufsize_t *content_start, bufsize_t *content_end,
                               int *is_folded) {
  if (len < 4) {
    return 0;
  }

  bufsize_t left = 0;
  while (left < len && line[left] == '=') {
    left++;
  }

  if (left == 0 || left > 6 || left >= len) {
    return 0;
  }

  bool folded_left = false;
  if (line[left] == '#') {
    folded_left = true;
    left++;
  }

  if (left >= len || line[left] != ' ') {
    return 0;
  }

  bufsize_t right = len;
  bufsize_t right_eq = 0;
  while (right > 0 && line[right - 1] == '=') {
    right--;
    right_eq++;
  }

  bool folded_right = false;
  if (right > 0 && line[right - 1] == '#') {
    folded_right = true;
    right--;
  }

  if (right_eq != left - (folded_left ? 1 : 0)) {
    return 0;
  }

  if (folded_left != folded_right) {
    return 0;
  }

  if (right == 0 || line[right - 1] != ' ') {
    return 0;
  }

  *content_start = left + 1;
  *content_end = right - 1;
  *is_folded = folded_left ? 1 : 0;

  if (*content_start > *content_end) {
    return 0;
  }

  return (int)(left - (folded_left ? 1 : 0));
}

static int is_horizontal_rule_line(const unsigned char *line, bufsize_t len) {
  /* Horizontal rules are deliberately recognized before paragraph fallback. */
  bufsize_t start = skip_spaces(line, len, 0);
  if (start >= len) {
    return 0;
  }

  int dash_count = 0;
  for (bufsize_t i = start; i < len; i++) {
    if (line[i] == '-') {
      dash_count++;
      continue;
    }
    if (isspace((unsigned char)line[i])) {
      continue;
    }
    return 0;
  }

  return dash_count >= 4 && dash_count <= 9;
}

/**
 * @brief Parse list marker, indentation, and starting number metadata.
 *
 * The parser stores indentation on list items rather than immediately building a
 * nested tree.  The renderer can then decide how to normalize mixed list/table
 * continuation cases without losing the original indentation signal.
 */
static int parse_list_prefix(const unsigned char *line, bufsize_t len, bufsize_t *text_start,
                             int *indent_level, namumark_list_marker_type *marker_type,
                             int *start_number) {
  bufsize_t i = 0;
  int spaces = 0;
  while (i < len && line[i] == ' ') {
    i++;
    spaces++;
  }

  if (i >= len) {
    return 0;
  }

  if (line[i] == '*') {
    i++;
    *marker_type = NAMUMARK_LIST_MARKER_BULLET;
    *start_number = 1;
  } else if (i + 1 < len && line[i + 1] == '.' &&
             (line[i] == '1' || line[i] == 'a' || line[i] == 'A' || line[i] == 'i' ||
              line[i] == 'I')) {
    switch (line[i]) {
      case '1':
        *marker_type = NAMUMARK_LIST_MARKER_NUMBER;
        break;
      case 'a':
        *marker_type = NAMUMARK_LIST_MARKER_ALPHA_LOWER;
        break;
      case 'A':
        *marker_type = NAMUMARK_LIST_MARKER_ALPHA_UPPER;
        break;
      case 'i':
        *marker_type = NAMUMARK_LIST_MARKER_ROMAN_LOWER;
        break;
      case 'I':
        *marker_type = NAMUMARK_LIST_MARKER_ROMAN_UPPER;
        break;
      default:
        *marker_type = NAMUMARK_LIST_MARKER_NONE;
        break;
    }
    i += 2;
    *start_number = 1;
    if (i < len && line[i] == '#') {
      int number = 0;
      int has_digits = 0;
      i++;
      while (i < len && isdigit((unsigned char)line[i])) {
        has_digits = 1;
        number = (number * 10) + (line[i] - '0');
        i++;
      }
      if (has_digits && number > 0) {
        *start_number = number;
      }
    }
  } else {
    return 0;
  }

  if (i < len && line[i] == ' ') {
    i++;
  }

  *text_start = i;
  *indent_level = spaces;
  return 1;
}

static int parse_blockquote_prefix(const unsigned char *line, bufsize_t len, bufsize_t *text_start,
                                   int *depth) {
  /* Consecutive '>' characters record nesting depth; one optional space is ignored. */
  bufsize_t i = 0;
  int count = 0;
  while (i < len && line[i] == '>') {
    i++;
    count++;
  }

  if (count == 0) {
    return 0;
  }

  if (i < len && line[i] == ' ') {
    i++;
  }

  *depth = count;
  *text_start = i;
  return 1;
}

/**
 * @brief Recognize redirect lines that terminate normal document parsing.
 *
 * Redirects must be the first meaningful line.  Once accepted, later lines are
 * ignored by process_line() to match NamuMark behavior and to keep category or
 * table syntax after a redirect from leaking into the AST.
 */
static int parse_redirect_line(const unsigned char *line, bufsize_t len, bufsize_t *target_start,
                               bufsize_t *target_end) {
  static const char redirect_en[] = "#redirect";
  static const char redirect_ko[] = "#넘겨주기";

  if (len >= (bufsize_t)strlen(redirect_en) &&
      memcmp(line, redirect_en, strlen(redirect_en)) == 0) {
    *target_start = strlen(redirect_en);
  } else if (len >= (bufsize_t)strlen(redirect_ko) &&
             memcmp(line, redirect_ko, strlen(redirect_ko)) == 0) {
    *target_start = strlen(redirect_ko);
  } else {
    return 0;
  }

  *target_end = len;
  trim_edges(line, target_start, target_end);
  return *target_start < *target_end;
}

static int parse_category_line(const unsigned char *line, bufsize_t len, bufsize_t *name_start,
                               bufsize_t *name_end) {
  /* Categories are only recognized when the entire line is a category link. */
  static const char prefix[] = "[[분류:";
  static const size_t prefix_len = sizeof(prefix) - 1;

  if (len < (bufsize_t)prefix_len + 2) {
    return 0;
  }

  if (memcmp(line, prefix, prefix_len) != 0) {
    return 0;
  }

  if (line[len - 2] != ']' || line[len - 1] != ']') {
    return 0;
  }

  *name_start = prefix_len;
  *name_end = len - 2;
  trim_edges(line, name_start, name_end);
  return *name_start < *name_end;
}

/**
 * @brief Append a category name to the document root.
 *
 * Category syntax may include display text after '|'.  The document-level list
 * stores only the actual category target so AST consumers do not need to repeat
 * the split logic.
 */
static int append_document_category(namumark_node *document, const unsigned char *name,
                                    bufsize_t len) {
  if (document == NULL || name == NULL || len == 0) {
    return 0;
  }

  bufsize_t actual_len = len;
  for (bufsize_t i = 0; i < len; i++) {
    if (name[i] == '|') {
      actual_len = i;
      break;
    }
  }
  while (actual_len > 0 && name[actual_len - 1] == ' ') {
    actual_len--;
  }
  if (actual_len == 0) {
    return 0;
  }

  if (document->category_count >= document->category_capacity) {
    int new_capacity = document->category_capacity == 0 ? 4 : document->category_capacity * 2;
    strbuf *categories = (strbuf *)realloc(document->categories, sizeof(strbuf) * new_capacity);
    if (categories == NULL) {
      return 0;
    }
    document->categories = categories;
    document->category_capacity = new_capacity;
  }

  strbuf_init(&document->categories[document->category_count], actual_len + 1);
  strbuf_set(&document->categories[document->category_count], name, actual_len);
  document->category_count++;
  return 1;
}

/**
 * @brief Parse a block footnote definition line.
 *
 * Inline footnotes are handled in inlines.c.  This helper only recognizes the
 * block form so the parser can attach label/content metadata before inline
 * rendering groups repeated references.
 */
static int parse_footnote_definition(const unsigned char *line, bufsize_t len,
                                     bufsize_t *label_start, bufsize_t *label_end,
                                     bufsize_t *text_start, bufsize_t *text_end) {
  if (len < 4 || line[0] != '[' || line[1] != '*') {
    return 0;
  }

  bufsize_t i = 2;
  while (i < len && line[i] != ']' && !isspace((unsigned char)line[i])) {
    i++;
  }

  if (i >= len) {
    return 0;
  }

  *label_start = 2;
  *label_end = i;

  while (i < len && line[i] != ']') {
    i++;
  }
  if (i >= len || line[i] != ']') {
    return 0;
  }

  i++;
  while (i < len && isspace((unsigned char)line[i])) {
    i++;
  }

  *text_start = i;
  *text_end = len;
  return 1;
}

static int is_table_row_start(const unsigned char *line, bufsize_t len) {
  /* Normal table rows always begin with double pipes after optional spaces. */
  bufsize_t i = skip_spaces(line, len, 0);
  return (i + 1 < len && line[i] == '|' && line[i + 1] == '|');
}

static int is_table_caption_start(const unsigned char *line, bufsize_t len) {
  /* Caption syntax starts with a single pipe and has a second pipe before cells. */
  bufsize_t i = skip_spaces(line, len, 0);
  if (i + 2 >= len || line[i] != '|' || line[i + 1] == '|') {
    return 0;
  }
  for (bufsize_t j = i + 1; j < len; j++) {
    if (line[j] == '|') {
      return j + 2 < len && line[j + 1] == ' ' && line[j + 2] != '|';
    }
  }
  return 0;
}

/*
 * Captions are table starts, but ordinary single-pipe prose is not.  This
 * preserves the NamuMark caption form "|caption| cells ||" without swallowing
 * a later paragraph such as "|not a table|" into the previous table.
 */
static int is_table_line_start(const unsigned char *line, bufsize_t len) {
  return is_table_row_start(line, len) || is_table_caption_start(line, len);
}

static int is_comment_only_line(const unsigned char *line, bufsize_t len) {
  /* Block comments can be indented; table-cell visible comments are handled later. */
  bufsize_t i = skip_spaces(line, len, 0);
  return i + 1 < len && line[i] == '#' && line[i + 1] == '#';
}

static int count_token(const unsigned char *line, bufsize_t len, const char *token,
                       bufsize_t token_len) {
  /* Simple non-overlapping delimiter count used for brace-depth bookkeeping. */
  int count = 0;
  if (token_len == 0 || len < token_len) {
    return 0;
  }
  for (bufsize_t i = 0; i + token_len <= len; i++) {
    if (memcmp(line + i, token, (size_t)token_len) == 0) {
      count++;
      i += token_len - 1;
    }
  }
  return count;
}

static bufsize_t find_block_token_in_range(const unsigned char *line, bufsize_t start,
                                           bufsize_t end, const char *token,
                                           bufsize_t token_len) {
  /* Local byte-range search; returns -1 so callers can use bufsize_t consistently. */
  if (line == NULL || token == NULL || token_len <= 0 || start >= end) {
    return -1;
  }

  for (bufsize_t i = start; i + token_len <= end; i++) {
    if (memcmp(line + i, token, (size_t)token_len) == 0) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Count wiki and non-wiki triple-brace depth changes on one line.
 *
 * We track {{{#!wiki}}} separately from other {{{...}}} blocks because wiki
 * blocks are block containers while syntax/style/literal braces can appear
 * inside them.  A close token first satisfies non-wiki depth; only then can it
 * close a wiki block.
 */
static void scan_wiki_line_depth(const unsigned char *line, bufsize_t len, int nonwiki_initial,
                                 int *wiki_starts, int *wiki_ends, int *nonwiki_final) {
  if (wiki_starts != NULL) {
    *wiki_starts = 0;
  }
  if (wiki_ends != NULL) {
    *wiki_ends = 0;
  }
  if (nonwiki_final != NULL) {
    *nonwiki_final = nonwiki_initial;
  }

  if (line == NULL || len < 3) {
    return;
  }

  int nonwiki_depth = nonwiki_initial;

  for (bufsize_t i = 0; i + 2 < len; i++) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      if (i + 9 <= len && memcmp(line + i, "{{{#!wiki", 9) == 0) {
        if (wiki_starts != NULL) {
          (*wiki_starts)++;
        }
      } else {
        nonwiki_depth++;
      }
      i += 2;
      continue;
    }

    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (nonwiki_depth > 0) {
        nonwiki_depth--;
      } else if (wiki_ends != NULL) {
        (*wiki_ends)++;
      }
      i += 2;
      continue;
    }
  }

  if (nonwiki_final != NULL) {
    *nonwiki_final = nonwiki_depth;
  }
}

/*
 * Only unclosed bare triple-brace text starts a block preformatted section.
 * A same-line close means the construct is an inline literal, e.g.
 * "{{{onclick}}} means ..." should become <code>onclick</code> inside a
 * paragraph rather than a whole <pre> block.
 */
static int is_inline_advanced_text_start(const unsigned char *line, bufsize_t len) {
  bufsize_t start = skip_spaces(line, len, 0);
  if (start + 3 > len || memcmp(line + start, "{{{", 3) != 0) {
    return 0;
  }
  if (start + 3 >= len) {
    return 1;
  }
  if (line[start + 3] == '#') {
    return 0;
  }
  if (find_block_token_in_range(line, start + 3, len, "}}}", 3) >= 0) {
    return 0;
  }
  return 1;
}

static int line_looks_like_block_start(const unsigned char *line, bufsize_t len) {
  /* Used to stop multiline inline spans before they swallow a new block. */
  if (line == NULL || len <= 0) {
    return 0;
  }

  bufsize_t s = skip_spaces(line, len, 0);
  if (s >= len) {
    return 0;
  }

  if (line[s] == '#') {
    return 1;
  }

  if (line[s] == '=') {
    return 1;
  }

  if (line[s] == '>') {
    return 1;
  }

  if (line[s] == '*' || line[s] == '@') {
    return 1;
  }

  if (s + 1 < len &&
      ((line[s] == '1' || line[s] == 'a' || line[s] == 'A' || line[s] == 'i' || line[s] == 'I') &&
       line[s + 1] == '.')) {
    return 1;
  }

  if (is_table_line_start(line, len)) {
    return 1;
  }

  return 0;
}

static int text_node_starts_with_token(const namumark_node *node, const char *token) {
  if (node == NULL || token == NULL) {
    return 0;
  }

  bufsize_t token_len = (bufsize_t)strlen(token);
  bufsize_t start = skip_spaces(node->content.ptr, node->content.size, 0);
  return start + token_len <= node->content.size &&
         memcmp(node->content.ptr + start, token, (size_t)token_len) == 0;
}

static int text_node_starts_with_any_token(const namumark_node *node, const char **tokens,
                                           size_t token_count) {
  for (size_t i = 0; i < token_count; i++) {
    if (text_node_starts_with_token(node, tokens[i])) {
      return 1;
    }
  }
  return 0;
}

static int text_node_contains_token(const namumark_node *node, const char *token) {
  if (node == NULL || token == NULL) {
    return 0;
  }

  bufsize_t token_len = (bufsize_t)strlen(token);
  if (token_len == 0 || node->content.size < token_len) {
    return 0;
  }

  for (bufsize_t i = 0; i + token_len <= node->content.size; i++) {
    if (memcmp(node->content.ptr + i, token, (size_t)token_len) == 0) {
      return 1;
    }
  }
  return 0;
}

static int is_line_only_advanced_end(const unsigned char *line, bufsize_t len) {
  /* Standalone }}} closes an advanced block without contributing content. */
  bufsize_t start = skip_spaces(line, len, 0);
  bufsize_t end = len;
  while (end > start && line[end - 1] == ' ') {
    end--;
  }
  return (end - start == 3 && line[start] == '}' && line[start + 1] == '}' && line[start + 2] == '}');
}

static int starts_with_wiki_block(const unsigned char *line, bufsize_t len) {
  /* Block-level {{{#!wiki}}} can be indented by spaces. */
  static const char prefix[] = "{{{#!wiki";
  static const bufsize_t prefix_len = sizeof(prefix) - 1;

  bufsize_t start = skip_spaces(line, len, 0);
  if (start + prefix_len > len) {
    return 0;
  }
  return memcmp(line + start, prefix, (size_t)prefix_len) == 0;
}

static void extract_wiki_block_attribute(const unsigned char *line, bufsize_t len,
                                         const char *attr, bufsize_t *value_start,
                                         bufsize_t *value_len);

static bufsize_t find_wiki_block_start_in_text(const unsigned char *line, bufsize_t len) {
  static const char prefix[] = "{{{#!wiki";
  static const bufsize_t prefix_len = sizeof(prefix) - 1;

  if (line == NULL || len < prefix_len) {
    return -1;
  }

  for (bufsize_t i = 0; i + prefix_len <= len; i++) {
    if (memcmp(line + i, prefix, (size_t)prefix_len) != 0) {
      continue;
    }
    if (i > 0 && line[i - 1] == '{') {
      continue;
    }
    return i;
  }

  return -1;
}

static int style_value_has_inline_display(const unsigned char *style, bufsize_t len) {
  if (style == NULL || len <= 0) {
    return 0;
  }

  for (bufsize_t i = 0; i + 7 <= len; i++) {
    if (tolower(style[i]) != 'd' || tolower(style[i + 1]) != 'i' ||
        tolower(style[i + 2]) != 's' || tolower(style[i + 3]) != 'p' ||
        tolower(style[i + 4]) != 'l' || tolower(style[i + 5]) != 'a' ||
        tolower(style[i + 6]) != 'y') {
      continue;
    }

    bufsize_t j = i + 7;
    while (j < len && (style[j] == ' ' || style[j] == '\t')) {
      j++;
    }
    if (j >= len || style[j] != ':') {
      continue;
    }

    j++;
    while (j < len && (style[j] == ' ' || style[j] == '\t')) {
      j++;
    }
    if (j + 6 > len) {
      continue;
    }

    if (tolower(style[j]) == 'i' && tolower(style[j + 1]) == 'n' &&
        tolower(style[j + 2]) == 'l' && tolower(style[j + 3]) == 'i' &&
        tolower(style[j + 4]) == 'n' && tolower(style[j + 5]) == 'e' &&
        (j + 6 >= len || style[j + 6] == ';' || isspace((unsigned char)style[j + 6]))) {
      return 1;
    }
  }

  return 0;
}

static int wiki_fragment_declares_inline_display(const unsigned char *line, bufsize_t len) {
  bufsize_t value_start = 0;
  bufsize_t value_len = 0;
  extract_wiki_block_attribute(line, len, "style", &value_start, &value_len);
  return style_value_has_inline_display(line + value_start, value_len);
}

static int wiki_fragment_has_class_attribute(const unsigned char *line, bufsize_t len) {
  bufsize_t value_start = 0;
  bufsize_t value_len = 0;
  extract_wiki_block_attribute(line, len, "class", &value_start, &value_len);
  for (bufsize_t i = 0; i < value_len; i++) {
    if (line[value_start + i] != ' ' && line[value_start + i] != '\t') {
      return 1;
    }
  }
  return 0;
}

static int wiki_attr_name_matches(const unsigned char *name, bufsize_t name_len,
                                  const char *attr) {
  /* Attribute names are byte-compared because grammar keywords are ASCII. */
  bufsize_t attr_len = (bufsize_t)strlen(attr);
  return name_len == attr_len && memcmp(name, attr, (size_t)attr_len) == 0;
}

static void extract_wiki_block_attribute(const unsigned char *line, bufsize_t len,
                                         const char *attr, bufsize_t *value_start,
                                         bufsize_t *value_len) {
  /* Mirrors inline wiki attribute extraction for block-level {{{#!wiki}}}. */
  static const char prefix[] = "{{{#!wiki";
  static const bufsize_t prefix_len = sizeof(prefix) - 1;

  *value_start = 0;
  *value_len = 0;

  bufsize_t pos = skip_spaces(line, len, 0);
  if (pos + prefix_len > len || memcmp(line + pos, prefix, (size_t)prefix_len) != 0) {
    return;
  }

  pos += prefix_len;
  while (pos < len && line[pos] == ' ') {
    pos++;
  }

  while (pos < len && line[pos] != '\n' && line[pos] != '\r') {
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) {
      pos++;
    }
    bufsize_t name_start = pos;
    while (pos < len && ((line[pos] >= 'a' && line[pos] <= 'z') ||
                         (line[pos] >= 'A' && line[pos] <= 'Z') ||
                         (line[pos] >= '0' && line[pos] <= '9') || line[pos] == '-')) {
      pos++;
    }
    bufsize_t name_len = pos - name_start;
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) {
      pos++;
    }
    if (name_len == 0 || pos >= len || line[pos] != '=') {
      return;
    }
    pos++;
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) {
      pos++;
    }

    bufsize_t val_start = pos;
    bufsize_t val_len = 0;
    if (pos < len && line[pos] == '"') {
      val_start = pos + 1;
      pos++;
      while (pos < len && line[pos] != '"') {
        pos++;
      }
      val_len = pos - val_start;
      if (pos < len && line[pos] == '"') {
        pos++;
      }
    } else {
      while (pos < len && line[pos] != ' ' && line[pos] != '\t' &&
             line[pos] != '\n' && line[pos] != '\r') {
        pos++;
      }
      val_len = pos - val_start;
    }

    if (wiki_attr_name_matches(line + name_start, name_len, attr)) {
      *value_start = val_start;
      *value_len = val_len;
      return;
    }
  }
}

static void extract_wiki_block_attributes(const unsigned char *line, bufsize_t len,
                                          namumark_node *wiki) {
  /* Store known attributes in generic node buffers consumed by renderers. */
  if (wiki == NULL) {
    return;
  }

  bufsize_t value_start = 0;
  bufsize_t value_len = 0;
  extract_wiki_block_attribute(line, len, "style", &value_start, &value_len);
  if (value_len > 0) {
    strbuf_set(&wiki->args, line + value_start, value_len);
  }
  extract_wiki_block_attribute(line, len, "class", &value_start, &value_len);
  if (value_len > 0) {
    strbuf_set(&wiki->label, line + value_start, value_len);
  }
  extract_wiki_block_attribute(line, len, "dark-style", &value_start, &value_len);
  if (value_len > 0) {
    strbuf_set(&wiki->target, line + value_start, value_len);
  }
  extract_wiki_block_attribute(line, len, "onclick", &value_start, &value_len);
  if (value_len > 0) {
    strbuf_set(&wiki->onclick, line + value_start, value_len);
  }
  extract_wiki_block_attribute(line, len, "tag", &value_start, &value_len);
  if (value_len > 0) {
    strbuf_set(&wiki->tag, line + value_start, value_len);
  }
}

/**
 * @brief Decide how many trailing }}} tokens structurally close a wiki block.
 *
 * Table rows can end with "}}}||" and nested literals can also end with braces.
 * This function separates structural wiki closes from literal closes so the
 * parser removes only the syntax wrapper and keeps example text intact.
 */
static int count_wiki_block_trailing_ends(const unsigned char *line, bufsize_t len,
                                          int nonwiki_initial, bufsize_t *content_end) {
  bufsize_t trimmed_end = len;
  while (trimmed_end > 0 && line[trimmed_end - 1] == ' ') {
    trimmed_end--;
  }

  bufsize_t scan_end = trimmed_end;
  while (scan_end >= 2 && line[scan_end - 2] == '|' && line[scan_end - 1] == '|') {
    scan_end -= 2;
    while (scan_end > 0 && line[scan_end - 1] == ' ') {
      scan_end--;
    }
  }

  bufsize_t close_scan_end = scan_end;
  int trailing_close_count = 0;
  while (close_scan_end >= 3 && line[close_scan_end - 3] == '}' && line[close_scan_end - 2] == '}' &&
         line[close_scan_end - 1] == '}') {
    trailing_close_count++;
    close_scan_end -= 3;
  }

  int nonwiki_brace_balance = nonwiki_initial;
  for (bufsize_t i = 0; i + 2 < close_scan_end;) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      if (!(i + 9 <= close_scan_end && memcmp(line + i, "{{{#!wiki", 9) == 0)) {
        nonwiki_brace_balance++;
      }
      i += 3;
      continue;
    }
    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (nonwiki_brace_balance > 0) {
        nonwiki_brace_balance--;
      }
      i += 3;
      continue;
    }
    i++;
  }

  int consume_nonwiki = 0;
  if (trailing_close_count > 0 && nonwiki_brace_balance > 0) {
    consume_nonwiki = (trailing_close_count < nonwiki_brace_balance)
                          ? trailing_close_count
                          : nonwiki_brace_balance;
  }

  int count = trailing_close_count - consume_nonwiki;
  bufsize_t end = trimmed_end;
  if (count > 0) {
    end = close_scan_end + (bufsize_t)consume_nonwiki * 3;
  }

  if (content_end != NULL) {
    *content_end = end;
  }

  int structural_count = 0;
  int nonwiki_depth = nonwiki_initial;

  for (bufsize_t i = 0; i + 2 < len;) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      if (!(i + 9 <= len && memcmp(line + i, "{{{#!wiki", 9) == 0)) {
        nonwiki_depth++;
      }
      i += 3;
      continue;
    }

    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (nonwiki_depth > 0) {
        nonwiki_depth--;
        i += 3;
        continue;
      }

      bufsize_t j = i + 3;
      while (j + 2 < len && line[j] == '}' && line[j + 1] == '}' && line[j + 2] == '}') {
        j += 3;
      }
      while (j < len && line[j] == ' ') {
        j++;
      }

      if (j >= len || (j + 1 < len && line[j] == '|' && line[j + 1] == '|')) {
        structural_count++;
      }

      i += 3;
      continue;
    }

    i++;
  }

  return structural_count;
}

static int starts_with_wiki_token_at(const unsigned char *line, bufsize_t len, bufsize_t pos) {
  /* Position-aware check used by close/reopen scans. */
  static const char token[] = "{{{#!wiki";
  static const bufsize_t token_len = sizeof(token) - 1;
  return (pos + token_len <= len && memcmp(line + pos, token, (size_t)token_len) == 0);
}

static int find_wiki_reopen_after_current_close(const unsigned char *line, bufsize_t len,
                                                int wiki_depth_initial,
                                                int nonwiki_depth_initial,
                                                bufsize_t *content_end,
                                                bufsize_t *tail_start) {
  int wiki_depth = wiki_depth_initial;
  int nonwiki_depth = nonwiki_depth_initial;

  if (line == NULL || wiki_depth <= 0 || len < 3) {
    return 0;
  }

  for (bufsize_t i = 0; i + 2 < len;) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      if (starts_with_wiki_token_at(line, len, i)) {
        wiki_depth++;
      } else {
        nonwiki_depth++;
      }
      i += 3;
      continue;
    }

    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (nonwiki_depth > 0) {
        nonwiki_depth--;
        i += 3;
        continue;
      }

      wiki_depth--;
      if (wiki_depth == 0) {
        bufsize_t after = i + 3;
        while (after < len && line[after] == ' ') {
          after++;
        }
        if (starts_with_wiki_token_at(line, len, after)) {
          if (content_end != NULL) {
            *content_end = i;
          }
          if (tail_start != NULL) {
            *tail_start = after;
          }
          return 1;
        }
      }
      i += 3;
      continue;
    }

    i++;
  }

  return 0;
}

/*
 * Same-line wiki tails are valid syntax.  For example, "}}}[clearfix]" closes
 * a wiki block and then emits a macro outside it.  We split at the close so the
 * braces do not leak into blockquote/table content.  Table row tails beginning
 * with "||" are intentionally left for the table parser.
 */
static int find_wiki_tail_after_current_close(const unsigned char *line, bufsize_t len,
                                              int wiki_depth_initial,
                                              int nonwiki_depth_initial,
                                              bufsize_t *content_end,
                                              bufsize_t *tail_start) {
  int wiki_depth = wiki_depth_initial;
  int nonwiki_depth = nonwiki_depth_initial;

  if (line == NULL || wiki_depth <= 0 || len < 3) {
    return 0;
  }

  for (bufsize_t i = 0; i + 2 < len;) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      if (starts_with_wiki_token_at(line, len, i)) {
        wiki_depth++;
      } else {
        nonwiki_depth++;
      }
      i += 3;
      continue;
    }

    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
      if (nonwiki_depth > 0) {
        nonwiki_depth--;
        i += 3;
        continue;
      }

      wiki_depth--;
      if (wiki_depth == 0) {
        bufsize_t after = i + 3;
        while (after < len && line[after] == ' ') {
          after++;
        }
        if (after < len) {
          if (after + 1 < len && line[after] == '|' && line[after + 1] == '|') {
            return 0;
          }
          if (content_end != NULL) {
            *content_end = i;
          }
          if (tail_start != NULL) {
            *tail_start = after;
          }
          return 1;
        }
        return 0;
      }
      i += 3;
      continue;
    }

    i++;
  }

  return 0;
}

static int line_ends_with_table_sep(const unsigned char *line, bufsize_t len) {
  /* Detect whether a table row is complete even when wiki closes precede ||. */
  if (len < 2) {
    return 0;
  }

  bufsize_t start = skip_spaces(line, len, 0);
  if (start + 1 >= len || line[start] != '|' || line[start + 1] != '|') {
    return 0;
  }

  bufsize_t last_sep = -1;
  for (bufsize_t i = start; i + 1 < len; i++) {
    if (line[i] == '|' && line[i + 1] == '|') {
      last_sep = i;
      i++;
    }
  }

  if (last_sep < 0) {
    return 0;
  }

  bufsize_t tail = last_sep + 2;
  while (tail < len && line[tail] == ' ') {
    tail++;
  }

  while (tail < len && line[tail] == '}') {
    tail++;
  }

  while (tail < len && line[tail] == ' ') {
    tail++;
  }

  return tail >= len;
}

static int line_has_trailing_table_sep(const unsigned char *line, bufsize_t len) {
  /* Faster suffix check for branches that only need to know whether || trails. */
  if (len < 2) {
    return 0;
  }

  bufsize_t end = len;
  while (end > 0 && line[end - 1] == ' ') {
    end--;
  }
  if (end >= 2 && line[end - 2] == '|' && line[end - 1] == '|') {
    return 1;
  }
  if (end >= 5 && line[end - 5] == '}' && line[end - 4] == '}' && line[end - 3] == '}' &&
      line[end - 2] == '|' && line[end - 1] == '|') {
    return 1;
  }
  return 0;
}

static int is_empty_table_row_line(const unsigned char *line, bufsize_t len) {
  /* A bare || line closes certain multiline table-row examples. */
  bufsize_t start = skip_spaces(line, len, 0);
  bufsize_t end = len;
  while (end > start && line[end - 1] == ' ') {
    end--;
  }
  return end == start + 2 && line[start] == '|' && line[start + 1] == '|';
}

static int table_content_ends_with_wiki_close(const namumark_node *table) {
  /* Helps distinguish a row-ending || from a wiki close followed by ||. */
  if (table == NULL || table->content.size < 3) {
    return 0;
  }

  bufsize_t end = table->content.size;
  while (end > 0 && (table->content.ptr[end - 1] == ' ' || table->content.ptr[end - 1] == '\n' ||
                     table->content.ptr[end - 1] == '\r')) {
    end--;
  }
  return end >= 3 && table->content.ptr[end - 3] == '}' && table->content.ptr[end - 2] == '}' &&
         table->content.ptr[end - 1] == '}';
}

/**
 * @brief Update nested advanced depth while collecting a table row.
 *
 * A table cell may contain {{{#!wiki}}}, {{{#!folding}}}, or a literal table
 * example.  While any of those are open, lines beginning with || belong to the
 * cell, not to the outer document table.
 */
static void update_table_wiki_depth(namumark_parser *parser, const unsigned char *line,
                                    bufsize_t len) {
  int starts = 0;
  int ends = 0;
  int nonwiki_final = parser->table_wiki_nonwiki_depth;
  scan_wiki_line_depth(line, len, parser->table_wiki_nonwiki_depth, &starts, &ends,
                       &nonwiki_final);
  parser->table_wiki_nonwiki_depth = nonwiki_final;
  parser->table_wiki_block_depth += starts;
  parser->table_wiki_block_depth -= ends;
  if (parser->table_wiki_block_depth < 0) {
    parser->table_wiki_block_depth = 0;
  }
}

static void open_wiki_block_from_fragment(namumark_parser *parser, const unsigned char *line,
                                          bufsize_t len);

static void append_text_tail_as_block(namumark_parser *parser, const unsigned char *line,
                                      bufsize_t len) {
  /* Tail text after a same-line close is parsed immediately as its own block. */
  if (parser == NULL || line == NULL || len <= 0) {
    return;
  }

  namumark_node *text = append_block_text(parser, NAMUMARK_NODE_TEXT, line, len);
  if (text != NULL) {
    parse_inlines(&text->content, text, parser->line_number);
  }
}

static void process_advanced_close_tail(namumark_parser *parser, const unsigned char *tail_line,
                                        bufsize_t tail_len, bufsize_t line_len) {
  /* Dispatch the text that follows a }}} close on the same physical line. */
  if (parser == NULL || tail_line == NULL || tail_len <= 0) {
    return;
  }

  if (starts_with_wiki_block(tail_line, tail_len)) {
    open_wiki_block_from_fragment(parser, tail_line, tail_len);
    return;
  }

  if (is_inline_advanced_text_start(tail_line, tail_len)) {
    namumark_node *pre = append_block_text(parser, NAMUMARK_NODE_PREFORMATTED, NULL, 0);
    if (pre != NULL) {
      pre->end_line = parser->line_number;
      pre->end_column = (int)line_len;

      bufsize_t start_idx = skip_spaces(tail_line, tail_len, 0) + 3;
      if (start_idx < tail_len) {
        bufsize_t content_len = tail_len - start_idx;
        int closes = count_token(tail_line + start_idx, content_len, "}}}", 3);
        if (closes > 0) {
          bufsize_t trim_end = tail_len;
          while (trim_end > start_idx && tail_line[trim_end - 1] == ' ') {
            trim_end--;
          }
          while (trim_end >= start_idx + 3 && tail_line[trim_end - 3] == '}' &&
                 tail_line[trim_end - 2] == '}' && tail_line[trim_end - 1] == '}') {
            trim_end -= 3;
          }
          while (trim_end > start_idx && tail_line[trim_end - 1] == ' ') {
            trim_end--;
          }
          if (trim_end > start_idx) {
            strbuf_put(&pre->content, tail_line + start_idx, trim_end - start_idx);
          }
        } else {
          strbuf_put(&pre->content, tail_line + start_idx, content_len);
        }

        parser->advanced_brace_depth = 1;
        parser->advanced_brace_depth += count_token(tail_line + start_idx, tail_len - start_idx,
                                                    "{{{", 3);
        parser->advanced_brace_depth -= count_token(tail_line + start_idx, tail_len - start_idx,
                                                    "}}}", 3);
        if (parser->advanced_brace_depth < 0) {
          parser->advanced_brace_depth = 0;
        }

        if (parser->advanced_brace_depth > 0) {
          parser->advanced_text_node = pre;
        } else {
          parser->advanced_text_node = NULL;
        }
      }
    }
    return;
  }

  append_text_tail_as_block(parser, tail_line, tail_len);
}

static bufsize_t find_advanced_close_on_line(const unsigned char *line, bufsize_t len,
                                             int initial_depth) {
  /* Find an inline close while respecting nested triple-brace depth. */
  if (line == NULL || len < 3 || initial_depth <= 0) {
    return -1;
  }

  int depth = initial_depth;
  for (bufsize_t i = 0; i + 2 < len;) {
    if (line[i] == '{' && line[i + 1] == '{' && line[i + 2] == '{') {
      depth++;
      i += 3;
      continue;
    }
    if (line[i] == '}' && line[i + 1] == '}' && line[i + 2] == '}') {
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

static void open_wiki_block_from_fragment(namumark_parser *parser, const unsigned char *line,
                                          bufsize_t len) {
  /* Open {{{#!wiki}}} from a full line or from tail text after a previous close. */
  if (parser == NULL || line == NULL || len <= 0) {
    return;
  }

  namumark_node *wiki = append_block_text(parser, NAMUMARK_NODE_WIKI_BLOCK, NULL, 0);
  if (wiki == NULL) {
    return;
  }
  wiki->start_column = (int)skip_spaces(line, len, 0) + 1;

  extract_wiki_block_attributes(line, len, wiki);

  bufsize_t ignored_end = 0;
  int starts = 0;
  int ends = 0;
  scan_wiki_line_depth(line, len, 0, &starts, &ends, &parser->wiki_nonwiki_depth);
  count_wiki_block_trailing_ends(line, len, 0, &ignored_end);

  parser->wiki_block_depth = starts - ends;
  if (parser->wiki_block_depth > 0) {
    parser->wiki_block_node = wiki;
  } else {
    parser->wiki_block_depth = 0;
    parser->wiki_block_node = NULL;
  }
}

namumark_node *make_document(void) {
  /* Documents use a normal node so renderers can share tree traversal code. */
  return make_block(NAMUMARK_NODE_DOCUMENT, 1, 1);
}

void process_line(namumark_parser *parser) {
  /*
   * The order of branches is part of the grammar.  Open containers are handled
   * before recognizing new block starts; otherwise nested table rows and same
   * line wiki closes would escape their owning block.
   */
  if (parser == NULL || parser->root == NULL) {
    return;
  }

  if (parser->line_number == 0 && parser->current_line.size >= 3 && parser->current_line.ptr[0] == 0xEF &&
      parser->current_line.ptr[1] == 0xBB && parser->current_line.ptr[2] == 0xBF) {
    strbuf_drop(&parser->current_line, 3);
  }

  trim_line_end(&parser->current_line);
  parser->line_number++;

  parser->last_line_length = parser->current_line.size;

  /* Redirect documents ignore everything after the redirect target. */
  if (parser->ignore_remaining_lines) {
    strbuf_clear(&parser->current_line);
    return;
  }

  const unsigned char *line = parser->current_line.ptr;
  bufsize_t len = parser->current_line.size;

  /* Empty lines either belong to an open container or split ordinary tables. */
  if (len == 0) {
    /*
     * Blank lines inside {{{#!wiki}}} are content.  They are especially
     * important because the wiki body is reparsed later, and blank lines split
     * adjacent nested tables.
     */
    if (parser->wiki_block_depth > 0 && parser->wiki_block_node != NULL) {
      if (parser->wiki_block_node->content.size > 0) {
        strbuf_putc(&parser->wiki_block_node->content, '\n');
      }
      parser->wiki_block_node->end_line = parser->line_number;
      parser->wiki_block_node->end_column = 0;
      strbuf_clear(&parser->current_line);
      return;
    }
    /*
     * Blank lines inside table-cell advanced syntax are also content.  Folding
     * examples often contain literal tables separated by blank lines; resetting
     * the table depth here would make inner rows escape the outer table.
     */
    if (parser->table_continuation && parser->root->last_child != NULL &&
        parser->root->last_child->type == NAMUMARK_NODE_TABLE &&
        (parser->table_wiki_block_depth > 0 || parser->table_wiki_nonwiki_depth > 0)) {
      namumark_node *table = parser->root->last_child;
      strbuf_putc(&table->content, '\n');
      table->end_line = parser->line_number;
      table->end_column = 0;
      strbuf_clear(&parser->current_line);
      return;
    }
    if (parser->root->last_child != NULL && parser->root->last_child->type == NAMUMARK_NODE_TABLE) {
      parser->table_interrupted_by_blank = true;
    }
    parser->table_continuation = false;
    parser->table_wiki_block_depth = 0;
    parser->table_wiki_nonwiki_depth = 0;
    strbuf_clear(&parser->current_line);
    return;
  }

  bufsize_t start = 0;
  bufsize_t end = len;

  /* Continue or close a block-level {{{#!wiki}}} before testing new blocks. */
  if (parser->wiki_block_depth > 0 && parser->wiki_block_node != NULL) {
    /*
     * Open {{{#!wiki}}} blocks receive first chance at every non-empty line.
     * They may close, reopen another wiki block on the same line, or emit tail
     * text such as [clearfix] after the close.
     */
    bufsize_t reopen_content_end = 0;
    bufsize_t reopen_tail_start = 0;
    /* Handle "}}} {{{#!wiki ..." where one wiki block closes and another opens. */
    if (find_wiki_reopen_after_current_close(line, len, parser->wiki_block_depth,
                                             parser->wiki_nonwiki_depth,
                                             &reopen_content_end, &reopen_tail_start)) {
      if (parser->wiki_block_node->content.size > 0) {
        strbuf_putc(&parser->wiki_block_node->content, '\n');
      }
      if (reopen_content_end > 0) {
        strbuf_put(&parser->wiki_block_node->content, line, reopen_content_end);
      }

      parser->wiki_block_node->end_line = parser->line_number;
      parser->wiki_block_node->end_column = (int)reopen_content_end;
      parser->wiki_block_depth = 0;
      parser->wiki_nonwiki_depth = 0;
      parser->wiki_block_node = NULL;

      open_wiki_block_from_fragment(parser, line + reopen_tail_start, len - reopen_tail_start);
      strbuf_clear(&parser->current_line);
      return;
    }

    bufsize_t tail_content_end = 0;
    bufsize_t tail_start = 0;
    /* Handle "}}}[macro]" tails that must render outside the closing wiki block. */
    if (find_wiki_tail_after_current_close(line, len, parser->wiki_block_depth,
                                           parser->wiki_nonwiki_depth,
                                           &tail_content_end, &tail_start)) {
      if (parser->wiki_block_node->content.size > 0) {
        strbuf_putc(&parser->wiki_block_node->content, '\n');
      }
      if (tail_content_end > 0) {
        strbuf_put(&parser->wiki_block_node->content, line, tail_content_end);
      }

      parser->wiki_block_node->end_line = parser->line_number;
      parser->wiki_block_node->end_column = (int)tail_content_end;
      parser->wiki_block_depth = 0;
      parser->wiki_nonwiki_depth = 0;
      parser->wiki_block_node = NULL;

      process_advanced_close_tail(parser, line + tail_start, len - tail_start, len);
      strbuf_clear(&parser->current_line);
      return;
    }

    bufsize_t s = skip_spaces(line, len, 0);
    /* A line beginning with a close may immediately start the next wiki block. */
    if (parser->wiki_block_depth == 1 && s + 3 <= len && memcmp(line + s, "}}}", 3) == 0) {
      bufsize_t after = s + 3;
      while (after < len && line[after] == ' ') {
        after++;
      }

      if (after < len && starts_with_wiki_token_at(line, len, after)) {
        parser->wiki_block_node->end_line = parser->line_number;
        parser->wiki_block_node->end_column = (int)len;

        namumark_node *next_wiki = append_block_text(parser, NAMUMARK_NODE_WIKI_BLOCK, NULL, 0);
        if (next_wiki != NULL) {
          extract_wiki_block_attributes(line + after, len - after, next_wiki);

          bufsize_t ignored_end = 0;
          int starts_after = 0;
          int ends_after = 0;
          scan_wiki_line_depth(line + after, len - after, 0, &starts_after, &ends_after,
                               &parser->wiki_nonwiki_depth);
          count_wiki_block_trailing_ends(line + after, len - after, 0, &ignored_end);
          parser->wiki_block_depth = starts_after - ends_after;
          if (parser->wiki_block_depth > 0) {
            parser->wiki_block_node = next_wiki;
          } else {
            parser->wiki_block_depth = 0;
            parser->wiki_nonwiki_depth = 0;
            parser->wiki_block_node = NULL;
          }
        }

        strbuf_clear(&parser->current_line);
        return;
      }
    }

    int starts = 0;
    int ends = 0;
    bufsize_t content_start = 0;
    bufsize_t content_end = len;
    int omit_line = 0;
    int counted_trailing = 0;
    int nonwiki_depth_before = parser->wiki_nonwiki_depth;

    bufsize_t seg_start = skip_spaces(line, len, 0);
    if (parser->wiki_block_depth == 1 && seg_start + 3 <= len && memcmp(line + seg_start, "}}}", 3) == 0) {
      bufsize_t after = seg_start + 3;
      while (after < len && line[after] == ' ') {
        after++;
      }

      if (after >= len && nonwiki_depth_before <= 0) {
        ends = 1;
        omit_line = 1;
      } else if (after >= len) {
        /* This closes a nested non-wiki advanced block such as #!style, not the wiki block. */
      } else if (starts_with_wiki_token_at(line, len, after)) {
        ends = 1;
        scan_wiki_line_depth(line + after, len - after, 0, &starts, &ends, &parser->wiki_nonwiki_depth);
        ends = 1 + ends;
        bufsize_t trailing_end = 0;
        count_wiki_block_trailing_ends(line + after, len - after, 0, &trailing_end);
        content_start = after;
        content_end = after + trailing_end;
      }
    }

    /* Preserve closes for nested non-wiki advanced blocks, such as #!style. */
    if (starts == 0 && ends == 0 && nonwiki_depth_before > 0 &&
        is_line_only_advanced_end(line + content_start, content_end - content_start)) {
      scan_wiki_line_depth(line + content_start, content_end - content_start,
                           parser->wiki_nonwiki_depth, &starts, &ends,
                           &parser->wiki_nonwiki_depth);
      content_end = len;
    } else if (starts == 0 && ends == 0) {
      scan_wiki_line_depth(line + content_start, content_end - content_start,
                           parser->wiki_nonwiki_depth, &starts, &ends,
                           &parser->wiki_nonwiki_depth);
      bufsize_t trailing_end = 0;
      count_wiki_block_trailing_ends(line + content_start, content_end - content_start,
                                      nonwiki_depth_before, &trailing_end);
      if (skip_spaces(line + content_start, content_end - content_start, 0) <
          content_end - content_start) {
        content_end = content_start + trailing_end;
      }
      counted_trailing = 1;
    }

    int projected_depth = parser->wiki_block_depth + starts - ends;

    if (parser->wiki_nonwiki_depth > 0 && projected_depth <= 0) {
      projected_depth = 1;
      content_end = len;
    }

    if (counted_trailing && ends > 0 && projected_depth > 0) {
      content_end = len;
    }

    if (counted_trailing && ends > 0 && projected_depth <= 0 &&
        !line_has_trailing_table_sep(line, len)) {
      content_end = len;
      while (content_end > content_start && line[content_end - 1] == ' ') {
        content_end--;
      }
      if (content_end >= content_start + 3 && line[content_end - 3] == '}' &&
          line[content_end - 2] == '}' && line[content_end - 1] == '}') {
        content_end -= 3;
      }
    }

    if (!omit_line) {
      if (parser->wiki_block_node->content.size > 0) {
        strbuf_putc(&parser->wiki_block_node->content, '\n');
      }
      if (content_end > content_start) {
        strbuf_put(&parser->wiki_block_node->content, line + content_start,
                   content_end - content_start);
      }
    }

    parser->wiki_block_node->end_line = parser->line_number;
    parser->wiki_block_node->end_column = (int)len;

    parser->wiki_block_depth = projected_depth;
    if (parser->wiki_block_depth <= 0) {
      parser->wiki_block_depth = 0;
      parser->wiki_nonwiki_depth = 0;
      parser->wiki_block_node = NULL;
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  /* Continue an open block preformatted section. */
  if (parser->advanced_brace_depth > 0 && parser->advanced_text_node != NULL) {
    /*
     * Block preformatted text uses brace depth instead of line prefixes.  The
     * close may be alone or followed by more inline/block text, so tail handling
     * is shared with the wiki-close path.
     */
    bufsize_t adv_start = skip_spaces(line, len, 0);
    if (parser->advanced_brace_depth == 1 && adv_start + 3 <= len &&
        memcmp(line + adv_start, "}}}", 3) == 0) {
      bufsize_t tail = adv_start + 3;
      while (tail < len && line[tail] == ' ') {
        tail++;
      }

      parser->advanced_text_node->end_line = parser->line_number;
      parser->advanced_text_node->end_column = (int)len;
      parser->advanced_brace_depth = 0;
      parser->advanced_text_node = NULL;

      if (tail < len) {
        const unsigned char *tail_line = line + tail;
        bufsize_t tail_len = len - tail;
        process_advanced_close_tail(parser, tail_line, tail_len, len);
      }

      strbuf_clear(&parser->current_line);
      return;
    }

    int only_end_line = is_line_only_advanced_end(line, len);

    bufsize_t inline_close = find_advanced_close_on_line(line, len, parser->advanced_brace_depth);
    if (inline_close >= 0) {
      if (inline_close > 0 || !(only_end_line && parser->advanced_brace_depth == 1)) {
        if (parser->advanced_text_node->content.size > 0) {
          strbuf_putc(&parser->advanced_text_node->content, '\n');
        }
        if (inline_close > 0) {
          strbuf_put(&parser->advanced_text_node->content, line, inline_close);
        }
      }

      parser->advanced_text_node->end_line = parser->line_number;
      parser->advanced_text_node->end_column = (int)len;
      parser->advanced_brace_depth = 0;
      parser->advanced_text_node = NULL;

      bufsize_t tail = inline_close + 3;
      while (tail < len && line[tail] == ' ') {
        tail++;
      }
      if (tail < len) {
        process_advanced_close_tail(parser, line + tail, len - tail, len);
      }

      strbuf_clear(&parser->current_line);
      return;
    }

    if (!(only_end_line && parser->advanced_brace_depth == 1)) {
      if (parser->advanced_text_node->content.size > 0) {
        strbuf_putc(&parser->advanced_text_node->content, '\n');
      }
      strbuf_put(&parser->advanced_text_node->content, line, len);
    }
    parser->advanced_text_node->end_line = parser->line_number;
    parser->advanced_text_node->end_column = (int)len;

    parser->advanced_brace_depth += count_token(line, len, "{{{", 3);
    parser->advanced_brace_depth -= count_token(line, len, "}}}", 3);
    if (parser->advanced_brace_depth <= 0) {
      parser->advanced_brace_depth = 0;
      parser->advanced_text_node = NULL;
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  /* Continue a multiline inline advanced span unless a new block interrupts it. */
  if (parser->inline_advanced_depth > 0 && parser->inline_text_node != NULL) {
    /*
     * Multiline inline advanced text continues until either braces balance or a
     * new block clearly begins.  This prevents an unclosed inline literal from
     * swallowing a following table or heading.
     */
    static const char *block_tolerant_inline_advanced[] = {
        "{{{#!folding",
        "{{{#!style",
    };
    if (line_looks_like_block_start(line, len) &&
        !text_node_starts_with_any_token(parser->inline_text_node,
                                         block_tolerant_inline_advanced,
                                         sizeof(block_tolerant_inline_advanced) /
                                             sizeof(block_tolerant_inline_advanced[0])) &&
        !text_node_contains_token(parser->inline_text_node, "{{{#!style")) {
      parse_inlines(&parser->inline_text_node->content, parser->inline_text_node,
                    parser->line_number);
      parser->inline_advanced_depth = 0;
      parser->inline_text_node = NULL;
    } else {
      if (parser->inline_text_node->content.size > 0) {
        strbuf_putc(&parser->inline_text_node->content, '\n');
      }
      strbuf_put(&parser->inline_text_node->content, line, len);
      parser->inline_text_node->end_line = parser->line_number;
      parser->inline_text_node->end_column = (int)len;

      parser->inline_advanced_depth += count_token(line, len, "{{{", 3);
      parser->inline_advanced_depth -= count_token(line, len, "}}}", 3);
      if (parser->inline_advanced_depth <= 0) {
        parse_inlines(&parser->inline_text_node->content, parser->inline_text_node,
                      parser->line_number);
        parser->inline_advanced_depth = 0;
        parser->inline_text_node = NULL;
      }

      strbuf_clear(&parser->current_line);
      return;
    }
  }

  /* Preserve an empty row line that closes a table continuation. */
  if (parser->table_continuation && parser->root->last_child != NULL &&
      parser->root->last_child->type == NAMUMARK_NODE_TABLE &&
      is_empty_table_row_line(line, len) &&
      (parser->table_wiki_block_depth <= 0 || table_content_ends_with_wiki_close(parser->root->last_child))) {
    namumark_node *table = parser->root->last_child;
    strbuf_putc(&table->content, '\n');
    strbuf_put(&table->content, line, len);
    table->end_line = parser->line_number;
    table->end_column = (int)len;
    parser->table_continuation = false;
    parser->table_wiki_block_depth = 0;
    parser->table_wiki_nonwiki_depth = 0;

    strbuf_clear(&parser->current_line);
    return;
  }

  /* Append physical continuation lines to the previous table row. */
  if (parser->table_continuation && parser->root->last_child != NULL &&
      parser->root->last_child->type == NAMUMARK_NODE_TABLE &&
      ((!is_table_line_start(line, len) && line[skip_spaces(line, len, 0)] != '|') ||
       parser->table_wiki_nonwiki_depth > 0)) {
    namumark_node *table = parser->root->last_child;
    strbuf_putc(&table->content, '\n');
    strbuf_put(&table->content, line, len);
    table->end_line = parser->line_number;
    table->end_column = (int)len;

    update_table_wiki_depth(parser, line, len);
    if (parser->table_wiki_block_depth <= 0 && parser->table_wiki_nonwiki_depth <= 0 &&
        line_has_trailing_table_sep(line, len)) {
      parser->table_continuation = false;
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  /* Drop table-between comments without treating them as blank table splitters. */
  if (is_comment_only_line(line, len)) {
    /*
     * A comment line between table rows is absent for rendering and should not
     * behave like a blank line that splits the table.  Outside table context we
     * keep the old AST behavior and let the inline parser produce a comment.
     */
    if (parser->root->last_child != NULL && parser->root->last_child->type == NAMUMARK_NODE_TABLE) {
      strbuf_clear(&parser->current_line);
      return;
    }
  }

  /* Redirects are only valid as the first parsed line. */
  if (parser->line_number == 1 && parse_redirect_line(line, len, &start, &end)) {
    namumark_node *redirect = append_block_text(parser, NAMUMARK_NODE_REDIRECT, line + start, end - start);
    set_block_target(redirect, line + start, end - start);
    parser->ignore_remaining_lines = true;
    strbuf_clear(&parser->current_line);
    return;
  }

  /* Category declarations are document metadata and do not render as body text. */
  if (parse_category_line(line, len, &start, &end)) {
    append_document_category(parser->root, line + start, end - start);
    strbuf_clear(&parser->current_line);
    return;
  }

  /* Open a block-level {{{#!wiki}}} container. */
  if (starts_with_wiki_block(line, len)) {
    open_wiki_block_from_fragment(parser, line, len);
    strbuf_clear(&parser->current_line);
    return;
  }

  /* Column-zero unclosed bare triple braces start a preformatted block. */
  if (is_inline_advanced_text_start(line, len) && skip_spaces(line, len, 0) == 0) {
    namumark_node *pre = append_block_text(parser, NAMUMARK_NODE_PREFORMATTED, NULL, 0);
    if (pre != NULL) {
      pre->end_line = parser->line_number;
      pre->end_column = (int)len;

      bufsize_t start_idx = skip_spaces(line, len, 0) + 3;
      if (start_idx < len) {
        bufsize_t content_len = len - start_idx;
        int closes = count_token(line + start_idx, content_len, "}}}", 3);
        if (closes > 0) {
          bufsize_t trim_end = len;
          while (trim_end > start_idx && line[trim_end - 1] == ' ') {
            trim_end--;
          }
          while (trim_end >= start_idx + 3 && line[trim_end - 3] == '}' &&
                 line[trim_end - 2] == '}' && line[trim_end - 1] == '}') {
            trim_end -= 3;
          }
          while (trim_end > start_idx && line[trim_end - 1] == ' ') {
            trim_end--;
          }
          if (trim_end > start_idx) {
            strbuf_put(&pre->content, line + start_idx, trim_end - start_idx);
          }
        } else {
          strbuf_put(&pre->content, line + start_idx, content_len);
        }
      }

      parser->advanced_brace_depth = 1;
      parser->advanced_brace_depth += count_token(line + start_idx, len - start_idx, "{{{", 3);
      parser->advanced_brace_depth -= count_token(line + start_idx, len - start_idx, "}}}", 3);
      if (parser->advanced_brace_depth < 0) {
        parser->advanced_brace_depth = 0;
      }

      if (parser->advanced_brace_depth > 0) {
        parser->advanced_text_node = pre;
      } else {
        parser->advanced_text_node = NULL;
      }
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  /* Four to nine dashes form a horizontal rule. */
  if (is_horizontal_rule_line(line, len)) {
    append_block_text(parser, NAMUMARK_NODE_HORIZONTAL_RULE, NULL, 0);
    strbuf_clear(&parser->current_line);
    return;
  }

  int is_folded = 0;
  int heading_level = parse_heading_level(line, len, &start, &end, &is_folded);
  /* Symmetric heading fences produce heading blocks. */
  if (heading_level > 0) {
    namumark_node *heading = append_block_text(parser, NAMUMARK_NODE_HEADING, line + start, end - start);
    if (heading != NULL) {
      heading->level = heading_level;
      heading->folded = is_folded;

      strbuf title;
      strbuf_init(&title, end - start + 1);
      strbuf_put(&title, line + start, end - start);
      parse_inlines(&title, heading, parser->line_number);
      strbuf_free(&title);
    }
    strbuf_clear(&parser->current_line);
    return;
  }

  int quote_depth = 0;
  /* A leading > creates a blockquote with recorded nesting depth. */
  if (parse_blockquote_prefix(line, len, &start, &quote_depth)) {
    namumark_node *quote = append_block_text(parser, NAMUMARK_NODE_BLOCKQUOTE, line + start, len - start);
    if (quote != NULL) {
      quote->depth = quote_depth;

      strbuf body;
      strbuf_init(&body, len - start + 1);
      strbuf_put(&body, line + start, len - start);
      parse_inlines(&body, quote, parser->line_number);
      strbuf_free(&body);
    }
    strbuf_clear(&parser->current_line);
    return;
  }

  int indent_level = 0;
  namumark_list_marker_type marker_type = NAMUMARK_LIST_MARKER_NONE;
  int start_number = 1;
  /* Lists keep marker and indent metadata for renderer-side nesting. */
  if (parse_list_prefix(line, len, &start, &indent_level, &marker_type, &start_number)) {
    namumark_node *list = append_block_text(parser, NAMUMARK_NODE_LIST, NULL, 0);
    if (list != NULL) {
      list->list_marker = marker_type;
      list->start_number = start_number;

      namumark_node *item = namumark_node_new(NAMUMARK_NODE_LIST_ITEM, parser->line_number, 1);
      if (item != NULL) {
        item->flags = (namumark_node_internal_flags)0;
        item->end_line = parser->line_number;
        item->end_column = (int)len;
        item->indent = indent_level;
        item->list_marker = marker_type;
        item->start_number = start_number;
        strbuf_set(&item->content, line + start, len - start);

        namumark_node_append_child(list, item);

        strbuf body;
        strbuf_init(&body, len - start + 1);
        strbuf_put(&body, line + start, len - start);
        int opens = count_token(body.ptr, body.size, "{{{", 3);
        int closes = count_token(body.ptr, body.size, "}}}", 3);
        if (opens > closes) {
          parser->inline_advanced_depth = opens - closes;
          parser->inline_text_node = item;
        } else {
          parse_inlines(&body, item, parser->line_number);
        }
        strbuf_free(&body);
      }
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  bufsize_t label_start = 0;
  bufsize_t label_end = 0;
  bufsize_t text_start = 0;
  bufsize_t text_end = 0;
  /* Block footnote definitions are parsed before generic table/text handling. */
  if (parse_footnote_definition(line, len, &label_start, &label_end, &text_start, &text_end)) {
    namumark_node *footnote = append_block_text(parser, NAMUMARK_NODE_FOOTNOTE_DEFINITION,
                                                line + text_start, text_end - text_start);
    if (footnote != NULL) {
      strbuf_set(&footnote->label, line + label_start, label_end - label_start);

      strbuf body;
      strbuf_init(&body, text_end - text_start + 1);
      strbuf_put(&body, line + text_start, text_end - text_start);
      parse_inlines(&body, footnote, parser->line_number);
      strbuf_free(&body);
    }
    strbuf_clear(&parser->current_line);
    return;
  }

  /* Start or append a document-level table row/caption. */
  if (is_table_line_start(line, len)) {
    /*
     * Table recognition comes after open-container handling.  At this point a
     * row start belongs to the document table, not to a nested wiki/folding cell.
     */
    if (!parser->table_continuation && is_empty_table_row_line(line, len) &&
        parser->root->last_child != NULL && parser->root->last_child->type == NAMUMARK_NODE_TABLE) {
      namumark_node *table = parser->root->last_child;
      strbuf_putc(&table->content, '\n');
      strbuf_put(&table->content, line, len);
      table->end_line = parser->line_number;
      table->end_column = (int)len;
      parser->table_wiki_block_depth = 0;
      parser->table_wiki_nonwiki_depth = 0;

      strbuf_clear(&parser->current_line);
      return;
    }

    if (!parser->table_continuation || parser->table_wiki_block_depth <= 0) {
      parser->table_wiki_block_depth = 0;
      parser->table_wiki_nonwiki_depth = 0;
    }
    update_table_wiki_depth(parser, line, len);
    parser->table_continuation = is_empty_table_row_line(line, len) ||
                                 parser->table_wiki_block_depth > 0 ||
                                 parser->table_wiki_nonwiki_depth > 0 ||
                                 !line_ends_with_table_sep(line, len);
    namumark_node *table = NULL;
    if (!parser->table_interrupted_by_blank && parser->root->last_child != NULL &&
        parser->root->last_child->type == NAMUMARK_NODE_TABLE) {
      table = parser->root->last_child;
      strbuf_putc(&table->content, '\n');
      strbuf_put(&table->content, line, len);
      table->end_line = parser->line_number;
      table->end_column = (int)len;
    } else {
      table = append_block_text(parser, NAMUMARK_NODE_TABLE, line, len);
    }
    parser->table_interrupted_by_blank = false;

    strbuf_clear(&parser->current_line);
    return;
  }

  parser->table_continuation = false;
  parser->table_interrupted_by_blank = false;
  parser->table_wiki_block_depth = 0;
  parser->table_wiki_nonwiki_depth = 0;

  bufsize_t embedded_wiki_start = find_wiki_block_start_in_text(line, len);
  if (embedded_wiki_start > 0 &&
      !wiki_fragment_declares_inline_display(line + embedded_wiki_start, len - embedded_wiki_start) &&
      !wiki_fragment_has_class_attribute(line + embedded_wiki_start, len - embedded_wiki_start)) {
    namumark_node *prefix = append_block_text(parser, NAMUMARK_NODE_TEXT, line, embedded_wiki_start);
    if (prefix != NULL) {
      parse_inlines(&prefix->content, prefix, parser->line_number);
    }
    open_wiki_block_from_fragment(parser, line + embedded_wiki_start, len - embedded_wiki_start);
    if (parser->root->last_child != NULL && parser->root->last_child->type == NAMUMARK_NODE_WIKI_BLOCK) {
      parser->root->last_child->start_column = (int)skip_spaces(line, len, 0) + 1;
    }
    strbuf_clear(&parser->current_line);
    return;
  }

  /* Fallback: ordinary paragraph text, possibly with a multiline inline span. */
  namumark_node *text = append_block_text(parser, NAMUMARK_NODE_TEXT, line, len);
  if (text != NULL) {
    int opens = count_token(line, len, "{{{", 3);
    int closes = count_token(line, len, "}}}", 3);
    if (opens > closes) {
      parser->inline_advanced_depth = opens - closes;
      parser->inline_text_node = text;
    } else {
      parse_inlines(&text->content, text, parser->line_number);
    }
  }

  strbuf_clear(&parser->current_line);
}

static void finalize_tree(namumark_node *node, int line_number, int column) {
  /* Fill missing end positions for nodes that were still open at EOF. */
  if (node == NULL) {
    return;
  }

  namumark_node *child = node->first_child;
  while (child != NULL) {
    namumark_node *next = child->next;
    finalize_tree(child, line_number, column);
    child = next;
  }

  node->flags &= ~NAMUMARK_NODE_OPEN;
  if (node->end_line < node->start_line) {
    node->end_line = line_number;
  }
  if (node->end_column <= 0) {
    node->end_column = column;
  }
}

namumark_node *finalize(namumark_parser *parser, namumark_node *block) {
  /* Finalize a single block and return its parent for stack-like callers. */
  if (block == NULL) {
    return NULL;
  }

  block->flags &= ~NAMUMARK_NODE_OPEN;
  if (parser != NULL) {
    block->end_line = parser->line_number;
    block->end_column = (int)parser->last_line_length;
  }

  return block->parent;
}

namumark_node *finalize_document(namumark_parser *parser) {
  /* The parser transfers root ownership to the caller after this point. */
  if (parser == NULL || parser->root == NULL) {
    return NULL;
  }

  finalize_tree(parser->root, parser->line_number, (int)parser->last_line_length);
  return parser->root;
}

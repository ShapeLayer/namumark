#include <ctype.h>
#include <string.h>

#include "blocks.h"
#include "inlines.h"
#include "node.h"
#include "parser.h"

static namumark_node *make_block(namumark_node_type node_type, int start_line, int start_column) {
  return namumark_node_new(node_type, start_line, start_column);
}

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
  while (from < len && s[from] == ' ') {
    from++;
  }
  return from;
}

static void trim_edges(const unsigned char *s, bufsize_t *start, bufsize_t *end) {
  while (*start < *end && isspace((unsigned char)s[*start])) {
    (*start)++;
  }
  while (*end > *start && isspace((unsigned char)s[*end - 1])) {
    (*end)--;
  }
}

static namumark_node *append_block_text(namumark_parser *parser, namumark_node_type type,
                                        const unsigned char *text, bufsize_t len) {
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
  if (node == NULL) {
    return;
  }
  strbuf_set(&node->target, text, len);
}

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
  bool folded_right = false;
  if (right > 0 && line[right - 1] == '#') {
    folded_right = true;
    right--;
  }

  bufsize_t right_eq = 0;
  while (right > 0 && line[right - 1] == '=') {
    right--;
    right_eq++;
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
  bufsize_t i = skip_spaces(line, len, 0);
  return (i + 1 < len && line[i] == '|' && line[i + 1] == '|');
}

static int count_token(const unsigned char *line, bufsize_t len, const char *token,
                       bufsize_t token_len) {
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
  return 1;
}

static int line_looks_like_block_start(const unsigned char *line, bufsize_t len) {
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

  if (is_table_row_start(line, len)) {
    return 1;
  }

  return 0;
}

static int is_line_only_advanced_end(const unsigned char *line, bufsize_t len) {
  bufsize_t start = skip_spaces(line, len, 0);
  bufsize_t end = len;
  while (end > start && line[end - 1] == ' ') {
    end--;
  }
  return (end - start == 3 && line[start] == '}' && line[start + 1] == '}' && line[start + 2] == '}');
}

static int starts_with_wiki_block(const unsigned char *line, bufsize_t len) {
  static const char prefix[] = "{{{#!wiki";
  static const bufsize_t prefix_len = sizeof(prefix) - 1;

  bufsize_t start = skip_spaces(line, len, 0);
  if (start + prefix_len > len) {
    return 0;
  }
  return memcmp(line + start, prefix, (size_t)prefix_len) == 0;
}

static void extract_wiki_block_style(const unsigned char *line, bufsize_t len,
                                     bufsize_t *style_start, bufsize_t *style_len) {
  static const char prefix[] = "{{{#!wiki";
  static const bufsize_t prefix_len = sizeof(prefix) - 1;

  *style_start = 0;
  *style_len = 0;

  bufsize_t pos = skip_spaces(line, len, 0);
  if (pos + prefix_len > len || memcmp(line + pos, prefix, (size_t)prefix_len) != 0) {
    return;
  }

  pos += prefix_len;
  while (pos < len && line[pos] == ' ') {
    pos++;
  }

  static const char style_key[] = "style=";
  static const bufsize_t style_key_len = sizeof(style_key) - 1;
  if (pos + style_key_len > len || memcmp(line + pos, style_key, (size_t)style_key_len) != 0) {
    return;
  }
  pos += style_key_len;

  if (pos >= len) {
    return;
  }

  if (line[pos] == '"') {
    bufsize_t q = pos + 1;
    while (q < len && line[q] != '"') {
      q++;
    }
    if (q > pos + 1 && q < len && line[q] == '"') {
      *style_start = pos + 1;
      *style_len = q - (pos + 1);
      return;
    }
  }

  bufsize_t end = len;
  while (end > pos && line[end - 1] == ' ') {
    end--;
  }
  if (end > pos) {
    *style_start = pos;
    *style_len = end - pos;
  }
}

static int count_wiki_block_starts(const unsigned char *line, bufsize_t len) {
  static const char token[] = "{{{#!wiki";
  static const bufsize_t token_len = sizeof(token) - 1;

  bufsize_t s = skip_spaces(line, len, 0);
  if (s + token_len > len) {
    return 0;
  }

  int has_table_sep = 0;
  for (bufsize_t i = s; i + 1 < len; i++) {
    if (line[i] == '|' && line[i + 1] == '|') {
      has_table_sep = 1;
      break;
    }
  }

  if (!is_table_row_start(line, len) && !has_table_sep) {
    return (memcmp(line + s, token, (size_t)token_len) == 0) ? 1 : 0;
  }

  int count = 0;
  for (bufsize_t i = s; i + token_len <= len; i++) {
    if (memcmp(line + i, token, (size_t)token_len) != 0) {
      continue;
    }

    if (i > 0 && line[i - 1] == '{') {
      continue;
    }

    int closes_on_same_line = 0;
    for (bufsize_t j = i + token_len; j + 2 < len; j++) {
      if (line[j] == '}' && line[j + 1] == '}' && line[j + 2] == '}') {
        closes_on_same_line = 1;
        break;
      }
    }

    if (!closes_on_same_line) {
      count++;
    }
  }

  return count;
}

static int count_wiki_block_trailing_ends(const unsigned char *line, bufsize_t len,
                                          bufsize_t *content_end) {
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

  int nonwiki_brace_balance = 0;
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
  int nonwiki_depth = 0;

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
  static const char token[] = "{{{#!wiki";
  static const bufsize_t token_len = sizeof(token) - 1;
  return (pos + token_len <= len && memcmp(line + pos, token, (size_t)token_len) == 0);
}

static int line_ends_with_table_sep(const unsigned char *line, bufsize_t len) {
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

static void append_text_tail_as_block(namumark_parser *parser, const unsigned char *line,
                                      bufsize_t len) {
  if (parser == NULL || line == NULL || len <= 0) {
    return;
  }

  namumark_node *text = append_block_text(parser, NAMUMARK_NODE_TEXT, line, len);
  if (text != NULL) {
    parse_inlines(&text->content, text, parser->line_number);
  }
}

static void open_wiki_block_from_fragment(namumark_parser *parser, const unsigned char *line,
                                          bufsize_t len) {
  if (parser == NULL || line == NULL || len <= 0) {
    return;
  }

  namumark_node *wiki = append_block_text(parser, NAMUMARK_NODE_WIKI_BLOCK, NULL, 0);
  if (wiki == NULL) {
    return;
  }

  bufsize_t style_start = 0;
  bufsize_t style_len = 0;
  extract_wiki_block_style(line, len, &style_start, &style_len);
  if (style_len > 0) {
    strbuf_set(&wiki->args, line + style_start, style_len);
  }

  bufsize_t ignored_end = 0;
  int starts = 0;
  int ends = 0;
  scan_wiki_line_depth(line, len, 0, &starts, &ends, &parser->wiki_nonwiki_depth);
  count_wiki_block_trailing_ends(line, len, &ignored_end);

  parser->wiki_block_depth = starts - ends;
  if (parser->wiki_block_depth > 0) {
    parser->wiki_block_node = wiki;
  } else {
    parser->wiki_block_depth = 0;
    parser->wiki_block_node = NULL;
  }
}

namumark_node *make_document(void) {
  return make_block(NAMUMARK_NODE_DOCUMENT, 1, 1);
}

void process_line(namumark_parser *parser) {
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

  if (parser->ignore_remaining_lines) {
    strbuf_clear(&parser->current_line);
    return;
  }

  const unsigned char *line = parser->current_line.ptr;
  bufsize_t len = parser->current_line.size;

  if (len == 0) {
    strbuf_clear(&parser->current_line);
    return;
  }

  bufsize_t start = 0;
  bufsize_t end = len;

  if (parser->wiki_block_depth > 0 && parser->wiki_block_node != NULL) {
    bufsize_t s = skip_spaces(line, len, 0);
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
          bufsize_t style_start = 0;
          bufsize_t style_len = 0;
          extract_wiki_block_style(line + after, len - after, &style_start, &style_len);
          if (style_len > 0) {
            strbuf_set(&next_wiki->args, line + after + style_start, style_len);
          }

          bufsize_t ignored_end = 0;
          int starts_after = 0;
          int ends_after = 0;
          scan_wiki_line_depth(line + after, len - after, 0, &starts_after, &ends_after,
                               &parser->wiki_nonwiki_depth);
          count_wiki_block_trailing_ends(line + after, len - after, &ignored_end);
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

    bufsize_t seg_start = skip_spaces(line, len, 0);
    if (parser->wiki_block_depth == 1 && seg_start + 3 <= len && memcmp(line + seg_start, "}}}", 3) == 0) {
      bufsize_t after = seg_start + 3;
      while (after < len && line[after] == ' ') {
        after++;
      }

      if (after >= len) {
        ends = 1;
        omit_line = 1;
      } else if (starts_with_wiki_token_at(line, len, after)) {
        ends = 1;
        scan_wiki_line_depth(line + after, len - after, 0, &starts, &ends, &parser->wiki_nonwiki_depth);
        ends = 1 + ends;
        bufsize_t trailing_end = 0;
        count_wiki_block_trailing_ends(line + after, len - after, &trailing_end);
        content_start = after;
        content_end = after + trailing_end;
      }
    }

    if (starts == 0 && ends == 0) {
      scan_wiki_line_depth(line + content_start, content_end - content_start,
                           parser->wiki_nonwiki_depth, &starts, &ends,
                           &parser->wiki_nonwiki_depth);
      bufsize_t trailing_end = 0;
      count_wiki_block_trailing_ends(line + content_start, content_end - content_start,
                                     &trailing_end);
      content_end = content_start + trailing_end;
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

  if (parser->advanced_brace_depth > 0 && parser->advanced_text_node != NULL) {
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

        if (starts_with_wiki_block(tail_line, tail_len)) {
          open_wiki_block_from_fragment(parser, tail_line, tail_len);
        } else if (is_inline_advanced_text_start(tail_line, tail_len)) {
          namumark_node *pre = append_block_text(parser, NAMUMARK_NODE_PREFORMATTED, NULL, 0);
          if (pre != NULL) {
            pre->end_line = parser->line_number;
            pre->end_column = (int)len;

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
            }

            parser->advanced_brace_depth = 1;
            parser->advanced_brace_depth +=
                count_token(tail_line + start_idx, tail_len - start_idx, "{{{", 3);
            parser->advanced_brace_depth -=
                count_token(tail_line + start_idx, tail_len - start_idx, "}}}", 3);
            if (parser->advanced_brace_depth < 0) {
              parser->advanced_brace_depth = 0;
            }

            if (parser->advanced_brace_depth > 0) {
              parser->advanced_text_node = pre;
            } else {
              parser->advanced_text_node = NULL;
            }
          }
        } else {
          append_text_tail_as_block(parser, tail_line, tail_len);
        }
      }

      strbuf_clear(&parser->current_line);
      return;
    }

    int only_end_line = is_line_only_advanced_end(line, len);
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

  if (parser->inline_advanced_depth > 0 && parser->inline_text_node != NULL) {
    if (line_looks_like_block_start(line, len)) {
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

  if (parser->table_continuation && parser->root->last_child != NULL &&
      parser->root->last_child->type == NAMUMARK_NODE_TABLE &&
      !is_table_row_start(line, len)) {
    namumark_node *table = parser->root->last_child;
    strbuf_putc(&table->content, '\n');
    strbuf_put(&table->content, line, len);
    table->end_line = parser->line_number;
    table->end_column = (int)len;

    if (line_ends_with_table_sep(line, len)) {
      parser->table_continuation = false;
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  if (parser->line_number == 1 && parse_redirect_line(line, len, &start, &end)) {
    namumark_node *redirect = append_block_text(parser, NAMUMARK_NODE_REDIRECT, line + start, end - start);
    set_block_target(redirect, line + start, end - start);
    parser->ignore_remaining_lines = true;
    strbuf_clear(&parser->current_line);
    return;
  }

  if (parse_category_line(line, len, &start, &end)) {
    namumark_node *category = append_block_text(parser, NAMUMARK_NODE_CATEGORY, line + start, end - start);
    set_block_target(category, line + start, end - start);
    strbuf_clear(&parser->current_line);
    return;
  }

  if (starts_with_wiki_block(line, len)) {
    open_wiki_block_from_fragment(parser, line, len);
    strbuf_clear(&parser->current_line);
    return;
  }

  if (is_inline_advanced_text_start(line, len)) {
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

  if (is_horizontal_rule_line(line, len)) {
    append_block_text(parser, NAMUMARK_NODE_HORIZONTAL_RULE, NULL, 0);
    strbuf_clear(&parser->current_line);
    return;
  }

  int is_folded = 0;
  int heading_level = parse_heading_level(line, len, &start, &end, &is_folded);
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
        parse_inlines(&body, item, parser->line_number);
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

  if (is_table_row_start(line, len)) {
    parser->table_continuation = !line_ends_with_table_sep(line, len);
    namumark_node *table = NULL;
    if (parser->root->last_child != NULL && parser->root->last_child->type == NAMUMARK_NODE_TABLE) {
      table = parser->root->last_child;
      strbuf_putc(&table->content, '\n');
      strbuf_put(&table->content, line, len);
      table->end_line = parser->line_number;
      table->end_column = (int)len;
    } else {
      table = append_block_text(parser, NAMUMARK_NODE_TABLE, line, len);
    }

    strbuf_clear(&parser->current_line);
    return;
  }

  parser->table_continuation = false;

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
  if (parser == NULL || parser->root == NULL) {
    return NULL;
  }

  finalize_tree(parser->root, parser->line_number, (int)parser->last_line_length);
  return parser->root;
}

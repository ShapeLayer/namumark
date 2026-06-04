#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inlines.h"
#include "parser.h"
#include "renderer.h"
#include "types.h"

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

static int print_html_escaped_bytes(FILE *out, const unsigned char *text, bufsize_t len) {
  strbuf buf;
  strbuf_init(&buf, len + 1);
  if (len > 0) {
    strbuf_set(&buf, text, len);
  }
  int ok = print_html_escaped(out, &buf);
  strbuf_free(&buf);
  return ok;
}

static int render_inline_node(FILE *out, const namumark_node *node);
static int render_block_node(FILE *out, const namumark_node *node);
static int render_block_children(FILE *out, const namumark_node *parent);
static int render_inline_snippet(FILE *out, const unsigned char *text, bufsize_t len);
static int render_table_cell_content(FILE *out, const unsigned char *text, bufsize_t len);

typedef struct footnote_entry {
  strbuf label;
  strbuf content;
  int *refs;
  int ref_count;
  int ref_capacity;
} footnote_entry;

typedef struct footnote_render_context {
  footnote_entry *entries;
  int entry_count;
  int entry_capacity;
  int next_ref;
  int rendered_count;
} footnote_render_context;

static footnote_render_context *active_footnotes = NULL;

static void footnote_context_init(footnote_render_context *ctx) {
  ctx->entries = NULL;
  ctx->entry_count = 0;
  ctx->entry_capacity = 0;
  ctx->next_ref = 0;
  ctx->rendered_count = 0;
}

static void footnote_context_free(footnote_render_context *ctx) {
  if (ctx == NULL) {
    return;
  }
  for (int i = 0; i < ctx->entry_count; i++) {
    strbuf_free(&ctx->entries[i].label);
    strbuf_free(&ctx->entries[i].content);
    free(ctx->entries[i].refs);
  }
  free(ctx->entries);
  ctx->entries = NULL;
  ctx->entry_count = 0;
  ctx->entry_capacity = 0;
}

static int strbuf_equals_bytes(const strbuf *buf, const unsigned char *data, bufsize_t len) {
  return buf != NULL && buf->size == len && (len == 0 || memcmp(buf->ptr, data, len) == 0);
}

static int footnote_find_entry(footnote_render_context *ctx, const unsigned char *label,
                               bufsize_t label_len) {
  for (int i = 0; i < ctx->entry_count; i++) {
    if (strbuf_equals_bytes(&ctx->entries[i].label, label, label_len)) {
      return i;
    }
  }
  return -1;
}

static footnote_entry *footnote_add_entry(footnote_render_context *ctx, const unsigned char *label,
                                          bufsize_t label_len, const unsigned char *content,
                                          bufsize_t content_len) {
  if (ctx->entry_count >= ctx->entry_capacity) {
    int new_capacity = ctx->entry_capacity == 0 ? 8 : ctx->entry_capacity * 2;
    footnote_entry *entries = (footnote_entry *)realloc(ctx->entries,
                                                        sizeof(footnote_entry) * new_capacity);
    if (entries == NULL) {
      return NULL;
    }
    ctx->entries = entries;
    ctx->entry_capacity = new_capacity;
  }

  footnote_entry *entry = &ctx->entries[ctx->entry_count++];
  memset(entry, 0, sizeof(*entry));
  strbuf_init(&entry->label, label_len + 1);
  strbuf_init(&entry->content, content_len + 1);
  strbuf_set(&entry->label, label, label_len);
  if (content != NULL && content_len > 0) {
    strbuf_set(&entry->content, content, content_len);
  }
  return entry;
}

static footnote_entry *footnote_get_or_add_entry(footnote_render_context *ctx,
                                                 const unsigned char *label,
                                                 bufsize_t label_len,
                                                 const unsigned char *content,
                                                 bufsize_t content_len) {
  int index = footnote_find_entry(ctx, label, label_len);
  if (index >= 0) {
    footnote_entry *entry = &ctx->entries[index];
    if (content != NULL && content_len > 0 && entry->content.size == 0) {
      strbuf_set(&entry->content, content, content_len);
    }
    return entry;
  }
  return footnote_add_entry(ctx, label, label_len, content, content_len);
}

static int footnote_add_ref(footnote_entry *entry, int ref_id) {
  if (entry->ref_count >= entry->ref_capacity) {
    int new_capacity = entry->ref_capacity == 0 ? 2 : entry->ref_capacity * 2;
    int *refs = (int *)realloc(entry->refs, sizeof(int) * new_capacity);
    if (refs == NULL) {
      return 0;
    }
    entry->refs = refs;
    entry->ref_capacity = new_capacity;
  }
  entry->refs[entry->ref_count++] = ref_id;
  return 1;
}

static int print_fragment_href(FILE *out, const strbuf *label) {
  static const char hex[] = "0123456789abcdef";
  for (bufsize_t i = 0; i < label->size; i++) {
    unsigned char c = label->ptr[i];
    int unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if (unreserved) {
      if (fputc((int)c, out) == EOF) {
        return 0;
      }
    } else {
      if (fputc('%', out) == EOF || fputc(hex[c >> 4], out) == EOF ||
          fputc(hex[c & 0x0f], out) == EOF) {
        return 0;
      }
    }
  }
  return 1;
}

static void parse_inline_footnote(const strbuf *raw, const unsigned char **label,
                                  bufsize_t *label_len, const unsigned char **content,
                                  bufsize_t *content_len, int *has_explicit_label) {
  *label = NULL;
  *label_len = 0;
  *content = raw->ptr;
  *content_len = raw->size;
  *has_explicit_label = 0;

  if (raw->size == 0 || isspace((unsigned char)raw->ptr[0])) {
    bufsize_t start = 0;
    while (start < raw->size && isspace((unsigned char)raw->ptr[start])) {
      start++;
    }
    *content = raw->ptr + start;
    *content_len = raw->size - start;
    return;
  }

  bufsize_t split = 0;
  while (split < raw->size && !isspace((unsigned char)raw->ptr[split])) {
    split++;
  }

  *label = raw->ptr;
  *label_len = split;
  *has_explicit_label = 1;

  if (split >= raw->size) {
    *content = raw->ptr + raw->size;
    *content_len = 0;
    return;
  }

  split++;
  while (split < raw->size && isspace((unsigned char)raw->ptr[split])) {
    split++;
  }
  *content = raw->ptr + split;
  *content_len = raw->size - split;
}

static int render_footnote_reference(FILE *out, const namumark_node *node) {
  if (active_footnotes == NULL) {
    return print_html_escaped(out, &node->content);
  }

  const unsigned char *label = NULL;
  const unsigned char *content = NULL;
  bufsize_t label_len = 0;
  bufsize_t content_len = 0;
  int has_explicit_label = 0;
  parse_inline_footnote(&node->content, &label, &label_len, &content, &content_len,
                        &has_explicit_label);

  int ref_id = ++active_footnotes->next_ref;
  char auto_label[32];
  if (!has_explicit_label) {
    int auto_len = snprintf(auto_label, sizeof(auto_label), "%d", ref_id);
    label = (const unsigned char *)auto_label;
    label_len = auto_len > 0 ? (bufsize_t)auto_len : 0;
  }

  footnote_entry *entry = footnote_get_or_add_entry(active_footnotes, label, label_len,
                                                    content, content_len);
  if (entry == NULL || !footnote_add_ref(entry, ref_id)) {
    return 0;
  }

  if (fputs("<a class=\"nm-footnote-ref\" href=\"#fn-", out) < 0 ||
      !print_fragment_href(out, &entry->label) || fprintf(out, "\"><span id=\"rfn-%d\"></span>[", ref_id) < 0 ||
      !print_html_escaped(out, &entry->label) || fputs("]</a>", out) < 0) {
    return 0;
  }
  return 1;
}

static int render_footnote_list(FILE *out) {
  if (active_footnotes == NULL || active_footnotes->rendered_count >= active_footnotes->entry_count) {
    return 1;
  }

  int start = active_footnotes->rendered_count;
  if (fputs("<div class=\"nm-footnotes\">\n", out) < 0) {
    return 0;
  }

  for (int i = start; i < active_footnotes->entry_count; i++) {
    footnote_entry *entry = &active_footnotes->entries[i];
    if (fputs("<span class=\"nm-footnote\"><span id=\"fn-", out) < 0 ||
        !print_html_escaped(out, &entry->label) || fputs("\"></span>", out) < 0) {
      return 0;
    }

    if (entry->ref_count <= 1) {
      if (entry->ref_count == 1) {
        if (fprintf(out, "<a href=\"#rfn-%d\">[", entry->refs[0]) < 0 ||
            !print_html_escaped(out, &entry->label) || fputs("]</a> ", out) < 0) {
          return 0;
        }
      } else if (fputc('[', out) == EOF || !print_html_escaped(out, &entry->label) ||
                 fputs("] ", out) < 0) {
        return 0;
      }
    } else {
      if (fputc('[', out) == EOF || !print_html_escaped(out, &entry->label) || fputs("] ", out) < 0) {
        return 0;
      }
      int first_ref = entry->refs[0];
      for (int ref_index = 0; ref_index < entry->ref_count; ref_index++) {
        if (fprintf(out, "<a href=\"#rfn-%d\"><sup>%d.%d</sup></a> ",
                    entry->refs[ref_index], first_ref, ref_index + 1) < 0) {
          return 0;
        }
      }
    }

    if (!render_inline_snippet(out, entry->content.ptr, entry->content.size) ||
        fputs("</span>\n", out) < 0) {
      return 0;
    }
  }

  active_footnotes->rendered_count = active_footnotes->entry_count;
  return fputs("</div>\n", out) >= 0;
}

static int is_list_continuation_text(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_TEXT && node->content.size > 0 &&
         node->content.ptr[0] == ' ';
}

static int is_list_continuation_table(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_TABLE && node->content.size > 0 &&
         node->content.ptr[0] == ' ';
}

static int is_list_continuation_wiki_block(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_WIKI_BLOCK && node->start_column > 1;
}

static int is_rendered_list_sequence_node(const namumark_node *node) {
  return node != NULL && (node->type == NAMUMARK_NODE_LIST || is_list_continuation_text(node) ||
                          is_list_continuation_table(node) || is_list_continuation_wiki_block(node));
}

static int is_footnote_macro_only_text(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_TEXT && node->first_child != NULL &&
         node->first_child == node->last_child && node->first_child->type == NAMUMARK_NODE_MACRO &&
          node->first_child->macro_type == NAMUMARK_NODE_MACRO_FOOTNOTE;
}

static int is_block_macro(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_MACRO &&
         (node->macro_type == NAMUMARK_NODE_MACRO_FOOTNOTE ||
          node->macro_type == NAMUMARK_NODE_MACRO_CLEARFIX);
}

static int text_contains_block_macro(const namumark_node *node) {
  if (node == NULL || node->type != NAMUMARK_NODE_TEXT) {
    return 0;
  }
  const namumark_node *child = node->first_child;
  while (child != NULL) {
    if (is_block_macro(child)) {
      return 1;
    }
    child = child->next;
  }
  return 0;
}

static int render_text_with_block_macro(FILE *out, const namumark_node *node) {
  const namumark_node *child = node->first_child;
  int paragraph_open = 0;

  while (child != NULL) {
    if (is_block_macro(child)) {
      if (paragraph_open) {
        if (fputs("</p>\n", out) < 0) {
          return 0;
        }
        paragraph_open = 0;
      }
      if (!render_inline_node(out, child)) {
        return 0;
      }
    } else {
      if (!paragraph_open) {
        if (fputs("<p>", out) < 0) {
          return 0;
        }
        paragraph_open = 1;
      }
      if (!render_inline_node(out, child)) {
        return 0;
      }
    }
    child = child->next;
  }

  if (paragraph_open && fputs("</p>\n", out) < 0) {
    return 0;
  }
  return 1;
}

static int is_style_advanced_only_text(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_TEXT && node->first_child != NULL &&
         node->first_child == node->last_child &&
         node->first_child->type == NAMUMARK_NODE_ADVANCED &&
         node->first_child->advanced_type == NAMUMARK_NODE_ADVANCED_STYLE;
}

static int is_blockquote_sequence_node(const namumark_node *node) {
  return node != NULL && node->type == NAMUMARK_NODE_BLOCKQUOTE;
}

static int count_leading_spaces_in_node(const namumark_node *node) {
  if (node != NULL && node->type == NAMUMARK_NODE_WIKI_BLOCK && node->start_column > 1) {
    return node->start_column - 1;
  }
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

static int render_text_with_line_breaks(FILE *out, const strbuf *content) {
  if (content == NULL) {
    return 1;
  }

  bufsize_t start = 0;
  for (bufsize_t i = 0; i < content->size; i++) {
    if (content->ptr[i] == '\n') {
      if (i > start && !print_html_escaped_bytes(out, content->ptr + start, i - start)) {
        return 0;
      }
      if (fputs("<br />", out) < 0) {
        return 0;
      }
      start = i + 1;
    }
  }

  if (start < content->size &&
      !print_html_escaped_bytes(out, content->ptr + start, content->size - start)) {
    return 0;
  }
  return 1;
}

static int render_inline_node_with_line_breaks(FILE *out, const namumark_node *node) {
  if (node->type == NAMUMARK_NODE_TEXT) {
    return render_text_with_line_breaks(out, &node->content);
  }
  return render_inline_node(out, node);
}

static int render_inline_children_with_line_breaks(FILE *out, const namumark_node *parent) {
  if (parent->first_child == NULL) {
    return render_text_with_line_breaks(out, &parent->content);
  }

  const namumark_node *child = parent->first_child;
  while (child != NULL) {
    if (!render_inline_node_with_line_breaks(out, child)) {
      return 0;
    }
    child = child->next;
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

  int ok = render_inline_children_with_line_breaks(out, &fake_parent);

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

static int render_file_link(FILE *out, const namumark_node *node) {
  if (fputs("<img src=\"", out) < 0 || !print_html_escaped(out, &node->target) ||
      fputs("\" alt=\"", out) < 0) {
    return 0;
  }
  const unsigned char *alt = node->target.ptr;
  bufsize_t alt_len = node->target.size;
  if (alt_len >= 7 && memcmp(alt, "파일:", 7) == 0) {
    alt += 7;
    alt_len -= 7;
  }
  if (!print_html_escaped_bytes(out, alt, alt_len)) {
    return 0;
  }
  if (fputc('"', out) == EOF) {
    return 0;
  }

  if (node->args.size > 0) {
    bufsize_t pos = 0;
    while (pos < node->args.size) {
      while (pos < node->args.size && (node->args.ptr[pos] == ' ' || node->args.ptr[pos] == '&')) {
        pos++;
      }
      bufsize_t key_start = pos;
      while (pos < node->args.size && node->args.ptr[pos] != '=' && node->args.ptr[pos] != '&') {
        pos++;
      }
      if (pos >= node->args.size || node->args.ptr[pos] != '=') {
        while (pos < node->args.size && node->args.ptr[pos] != '&') {
          pos++;
        }
        continue;
      }
      bufsize_t key_len = pos - key_start;
      pos++;
      bufsize_t value_start = pos;
      while (pos < node->args.size && node->args.ptr[pos] != '&') {
        pos++;
      }
      bufsize_t value_len = pos - value_start;
      if (key_len == 5 && memcmp(node->args.ptr + key_start, "width", 5) == 0 && value_len > 0) {
        if (fputs(" style=\"width:", out) < 0 ||
            !print_html_escaped_bytes(out, node->args.ptr + value_start, value_len)) {
          return 0;
        }
        int numeric = 1;
        for (bufsize_t i = 0; i < value_len; i++) {
          if (!isdigit((unsigned char)node->args.ptr[value_start + i])) {
            numeric = 0;
            break;
          }
        }
        if (numeric && fputs("px", out) < 0) {
          return 0;
        }
        if (fputc('"', out) == EOF) {
          return 0;
        }
        break;
      }
    }
  }

  return fputs(" />", out) >= 0;
}

static int render_ruby_macro(FILE *out, const namumark_node *node) {
  strbuf base;
  strbuf ruby;
  strbuf color;
  strbuf_init(&base, 32);
  strbuf_init(&ruby, 32);
  strbuf_init(&color, 16);

  bufsize_t pos = 0;
  int first = 1;
  while (pos <= node->args.size) {
    bufsize_t part_start = pos;
    while (pos < node->args.size && node->args.ptr[pos] != ',') {
      pos++;
    }
    bufsize_t part_end = pos;
    while (part_start < part_end && node->args.ptr[part_start] == ' ') {
      part_start++;
    }
    while (part_end > part_start && node->args.ptr[part_end - 1] == ' ') {
      part_end--;
    }

    if (part_end > part_start) {
      const unsigned char *eq = memchr(node->args.ptr + part_start, '=', (size_t)(part_end - part_start));
      if (eq == NULL && first) {
        strbuf_set(&base, node->args.ptr + part_start, part_end - part_start);
      } else if (eq != NULL) {
        bufsize_t key_len = (bufsize_t)(eq - (node->args.ptr + part_start));
        const unsigned char *value = eq + 1;
        bufsize_t value_len = part_end - (bufsize_t)(value - node->args.ptr);
        if (key_len == 4 && memcmp(node->args.ptr + part_start, "ruby", 4) == 0) {
          strbuf_set(&ruby, value, value_len);
        } else if (key_len == 5 && memcmp(node->args.ptr + part_start, "color", 5) == 0) {
          strbuf_set(&color, value, value_len);
        }
      }
    }

    first = 0;
    if (pos >= node->args.size) {
      break;
    }
    pos++;
  }

  int ok = 1;
  if (fputs("<ruby>", out) < 0 || !print_html_escaped(out, &base) ||
      fputs("<rp>(</rp><rt>", out) < 0) {
    ok = 0;
  }
  if (ok && color.size > 0) {
    if (fputs("<span style=\"color:", out) < 0 || !print_html_escaped(out, &color) ||
        fputs(";\">", out) < 0 || !print_html_escaped(out, &ruby) ||
        fputs("</span>", out) < 0) {
      ok = 0;
    }
  } else if (ok && !print_html_escaped(out, &ruby)) {
    ok = 0;
  }
  if (ok && fputs("</rt><rp>)</rp></ruby>", out) < 0) {
    ok = 0;
  }

  strbuf_free(&base);
  strbuf_free(&ruby);
  strbuf_free(&color);
  return ok;
}

static int render_youtube_macro(FILE *out, const namumark_node *node) {
  bufsize_t start = 0;
  bufsize_t end = node->args.size;
  while (start < end && (node->args.ptr[start] == ' ' || node->args.ptr[start] == '\t')) {
    start++;
  }
  while (end > start && (node->args.ptr[end - 1] == ' ' || node->args.ptr[end - 1] == '\t')) {
    end--;
  }

  if (fputs("<iframe allowfullscreen=\"\" width=\"640\" height=\"360\" frameborder=\"0\" src=\"//www.youtube.com/embed/", out) < 0) {
    return 0;
  }
  if (end > start && !print_html_escaped_bytes(out, node->args.ptr + start, end - start)) {
    return 0;
  }
  return fputs("\" loading=\"lazy\"></iframe>", out) >= 0;
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
        tolower(style->ptr[j + 4]) == 'n' && tolower(style->ptr[j + 5]) == 'e' &&
        (j + 6 >= style->size || style->ptr[j + 6] == ';' || isspace((unsigned char)style->ptr[j + 6]))) {
      return 1;
    }
  }

  return 0;
}

static int render_wiki_block_open(FILE *out, const char *tag, const namumark_node *node) {
  if (fprintf(out, "<%s class=\"nm-wiki-block", tag) < 0) {
    return 0;
  }
  if (node->label.size > 0) {
    if (fputc(' ', out) == EOF || !print_html_escaped(out, &node->label)) {
      return 0;
    }
  }
  if (fputc('"', out) == EOF) {
    return 0;
  }
  if (node->args.size > 0) {
    if (fputs(" style=\"", out) < 0 || !print_html_escaped(out, &node->args) ||
        fputs("\"", out) < 0) {
      return 0;
    }
  }
  if (node->target.size > 0) {
    if (fputs(" data-dark-style=\"", out) < 0 || !print_html_escaped(out, &node->target) ||
        fputs("\"", out) < 0) {
      return 0;
    }
  }
  if (node->onclick.size > 0) {
    if (fputs(" data-onclick=\"", out) < 0 || !print_html_escaped(out, &node->onclick) ||
        fputs("\"", out) < 0) {
      return 0;
    }
  }
  return fputc('>', out) != EOF;
}

static const char *wiki_render_tag(const namumark_node *node, int inline_mode) {
  if (node != NULL && node->tag.size == 1 && node->tag.ptr[0] == 'a') {
    return "a";
  }
  return inline_mode ? "span" : "div";
}

static bufsize_t wiki_advanced_body_start(const namumark_node *node) {
  bufsize_t start = 6;
  while (start < node->content.size && node->content.ptr[start] != '\n' &&
         node->content.ptr[start] != '\r') {
    start++;
  }
  while (start < node->content.size &&
         (node->content.ptr[start] == '\n' || node->content.ptr[start] == '\r')) {
    start++;
  }
  return start;
}

static int render_style_advanced_block(FILE *out, const namumark_node *node) {
  bufsize_t start = 7;
  while (start < node->content.size &&
         (node->content.ptr[start] == ' ' || node->content.ptr[start] == '\t')) {
    start++;
  }
  while (start < node->content.size &&
         (node->content.ptr[start] == '\n' || node->content.ptr[start] == '\r')) {
    start++;
  }

  if (fputs("<style>", out) < 0) {
    return 0;
  }
  bufsize_t end = node->content.size;
  while (end > start && (node->content.ptr[end - 1] == '\n' || node->content.ptr[end - 1] == '\r')) {
    end--;
  }
  if (start < end && !render_raw_bytes(out, node->content.ptr + start, end - start)) {
    return 0;
  }
  return fputs("</style>", out) >= 0;
}

static int render_wrapped_literal_body(FILE *out, const strbuf *body) {
  if (body == NULL || body->size < 6 || memcmp(body->ptr, "{{{", 3) != 0) {
    return 0;
  }

  bufsize_t end = body->size;
  while (end > 3 && (body->ptr[end - 1] == '\n' || body->ptr[end - 1] == '\r' ||
                     body->ptr[end - 1] == ' ')) {
    end--;
  }
  if (end < 6 || memcmp(body->ptr + end - 3, "}}}", 3) != 0) {
    return 0;
  }
  if (body->size > 4 && body->ptr[3] == '#' && body->ptr[4] == '!') {
    return 0;
  }
  if (body->ptr[3] == '+' || body->ptr[3] == '-') {
    return 0;
  }
  if (body->size > 4 && body->ptr[3] == '#' && body->ptr[4] != '#') {
    return 0;
  }

  if (fputs("<pre><code>", out) < 0 ||
      !render_html_entity_or_text(out, body->ptr + 3, end - 6) ||
      fputs("</code></pre>", out) < 0) {
    return 0;
  }
  return 1;
}

static bufsize_t find_token_in_range(const unsigned char *text, bufsize_t start,
                                     bufsize_t end, const char *token,
                                     bufsize_t token_len);

static int render_leading_style_blocks(FILE *out, strbuf *body) {
  while (body != NULL && body->size >= 10 && memcmp(body->ptr, "{{{#!style", 10) == 0) {
    bufsize_t close = find_token_in_range(body->ptr, 10, body->size, "}}}", 3);
    if (close < 0) {
      return 1;
    }

    namumark_node style_node;
    memset(&style_node, 0, sizeof(style_node));
    strbuf_init(&style_node.content, close + 1);
    strbuf_put(&style_node.content, body->ptr + 3, close - 3);
    int ok = render_style_advanced_block(out, &style_node);
    strbuf_free(&style_node.content);
    if (!ok) {
      return 0;
    }

    bufsize_t rest = close + 3;
    while (rest < body->size && (body->ptr[rest] == '\n' || body->ptr[rest] == '\r')) {
      rest++;
    }
    strbuf_drop(body, rest);
  }
  return 1;
}

static int render_syntax_advanced_block(FILE *out, const namumark_node *node) {
  bufsize_t line_end = strbuf_strchr(&node->content, '\n', 0);
  if (line_end < 0) {
    line_end = node->content.size;
  }

  bufsize_t lang_start = 8;
  while (lang_start < line_end &&
         (node->content.ptr[lang_start] == ' ' || node->content.ptr[lang_start] == '\t')) {
    lang_start++;
  }
  bufsize_t lang_end = line_end;
  while (lang_end > lang_start &&
         (node->content.ptr[lang_end - 1] == ' ' || node->content.ptr[lang_end - 1] == '\t' ||
          node->content.ptr[lang_end - 1] == '\r')) {
    lang_end--;
  }

  bufsize_t body_start = line_end < node->content.size ? line_end + 1 : node->content.size;
  if (body_start < node->content.size && node->content.ptr[body_start] == '\r') {
    body_start++;
  }
  bufsize_t body_end = node->content.size;
  while (body_end > body_start &&
         (node->content.ptr[body_end - 1] == ' ' || node->content.ptr[body_end - 1] == '\t')) {
    body_end--;
  }
  while (body_end > body_start &&
         (node->content.ptr[body_end - 1] == '\n' || node->content.ptr[body_end - 1] == '\r')) {
    body_end--;
  }

  if (fputs("<pre><code class=\"hljs\"", out) < 0) {
    return 0;
  }
  if (lang_end > lang_start) {
    if (fputs(" data-language=\"", out) < 0 ||
        !print_html_escaped_bytes(out, node->content.ptr + lang_start, lang_end - lang_start) ||
        fputs("\"", out) < 0) {
      return 0;
    }
  }
  if (fputs(">", out) < 0) {
    return 0;
  }
  if (body_end > body_start &&
      !print_html_escaped_bytes(out, node->content.ptr + body_start, body_end - body_start)) {
    return 0;
  }
  return fputs("</code></pre>", out) >= 0;
}

static int wiki_body_needs_block_rendering(const strbuf *body) {
  if (body == NULL) {
    return 0;
  }

  bufsize_t line_start = 0;
  while (line_start <= body->size) {
    bufsize_t line_end = line_start;
    while (line_end < body->size && body->ptr[line_end] != '\n') {
      line_end++;
    }

    bufsize_t s = line_start;
    while (s < line_end && body->ptr[s] == ' ') {
      s++;
    }
    if (s < line_end) {
      if (s + 3 <= line_end && memcmp(body->ptr + s, "{{{", 3) == 0 &&
          !(s + 4 <= line_end &&
            (body->ptr[s + 3] == '#' || body->ptr[s + 3] == '+' || body->ptr[s + 3] == '-'))) {
        if (find_token_in_range(body->ptr, s + 3, line_end, "}}}", 3) < 0) {
          return 1;
        }
      }
      if (s + 1 < line_end && body->ptr[s] == '|' && body->ptr[s + 1] == '|') {
        return 1;
      }
      if (body->ptr[s] == '=' || body->ptr[s] == '>' || body->ptr[s] == '*' || body->ptr[s] == '@') {
        return 1;
      }
      if (s + 1 < line_end && body->ptr[s + 1] == '.' &&
          (body->ptr[s] == '1' || body->ptr[s] == 'a' || body->ptr[s] == 'A' ||
           body->ptr[s] == 'i' || body->ptr[s] == 'I')) {
        return 1;
      }
    }

    if (line_end >= body->size) {
      break;
    }
    line_start = line_end + 1;
  }

  return 0;
}

static int render_wiki_advanced_block(FILE *out, const namumark_node *node) {
  int inline_mode = style_has_inline_display(&node->args);

  const char *tag = wiki_render_tag(node, inline_mode);
  if (!render_wiki_block_open(out, tag, node)) {
    return 0;
  }

  strbuf body;
  strbuf_init(&body, node->content.size + 1);
  bufsize_t start = wiki_advanced_body_start(node);

  if (start < node->content.size) {
    strbuf_set(&body, node->content.ptr + start, node->content.size - start);
    while (body.size > 0 && (body.ptr[body.size - 1] == '\n' || body.ptr[body.size - 1] == '\r')) {
      strbuf_truncate(&body, body.size - 1);
    }
  }

  if (inline_mode || !wiki_body_needs_block_rendering(&body)) {
    int ok = 0;
    if (body.size >= 6 && memcmp(body.ptr, "{{{", 3) == 0) {
      ok = render_wrapped_literal_body(out, &body);
    }
    if (!ok) {
      ok = render_advanced_content(out, &body);
    }
    strbuf_free(&body);
    if (!ok) {
      return 0;
    }
    return fprintf(out, "</%s>", tag) >= 0;
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

  return fprintf(out, "</%s>", tag) >= 0;
}

static int render_folding_advanced_block(FILE *out, const namumark_node *node) {
  if (node == NULL) {
    return 1;
  }

  bufsize_t line_end = strbuf_strchr(&node->content, '\n', 0);
  if (line_end < 0) {
    line_end = node->content.size;
  }

  bufsize_t summary_start = 9;
  while (summary_start < line_end &&
         (node->content.ptr[summary_start] == ' ' || node->content.ptr[summary_start] == '\t')) {
    summary_start++;
  }
  bufsize_t summary_end = line_end;
  while (summary_end > summary_start &&
         (node->content.ptr[summary_end - 1] == '\r' || node->content.ptr[summary_end - 1] == ' ')) {
    summary_end--;
  }

  if (fputs("<details class=\"nm-folding\"><summary>", out) < 0) {
    return 0;
  }
  if (summary_end > summary_start) {
    if (!print_html_escaped_bytes(out, node->content.ptr + summary_start, summary_end - summary_start)) {
      return 0;
    }
  } else if (fputs("More", out) < 0) {
    return 0;
  }
  if (fputs("</summary><div>", out) < 0) {
    return 0;
  }

  bufsize_t body_start = line_end < node->content.size ? line_end + 1 : node->content.size;
  if (body_start < node->content.size) {
    if (!render_table_cell_content(out, node->content.ptr + body_start, node->content.size - body_start)) {
      return 0;
    }
  }
  return fputs("</div></details>", out) >= 0;
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
      if (node->link_type == NAMUMARK_LINK_FILE && node->target.size > 0 &&
          node->target.ptr[0] != ':') {
        return render_file_link(out, node);
      }
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
    case NAMUMARK_NODE_FOOTNOTE_REFERENCE:
      return render_footnote_reference(out, node);
    case NAMUMARK_NODE_MACRO:
      if (node->macro_type == NAMUMARK_NODE_MACRO_BREAKLINE) {
        return fputs("<br />", out) >= 0;
      }
      if (node->macro_type == NAMUMARK_NODE_MACRO_FOOTNOTE) {
        return render_footnote_list(out);
      }
      if (node->macro_type == NAMUMARK_NODE_MACRO_CLEARFIX) {
        return fputs("<div class=\"nm-clearfix\"></div>", out) >= 0;
      }
      if (node->macro_type == NAMUMARK_NODE_MACRO_RUBY) {
        return render_ruby_macro(out, node);
      }
      if (node->macro_type == NAMUMARK_NODE_MACRO_YOUTUBE) {
        return render_youtube_macro(out, node);
      }
      if (node->target.size == 6 && memcmp(node->target.ptr, "anchor", 6) == 0) {
        return fputs("<a id=\"", out) >= 0 && print_html_escaped(out, &node->args) &&
               fputs("\"></a>", out) >= 0;
      }
      if (node->macro_type == NAMUMARK_NODE_MACRO_NONE) {
        if (fputc('[', out) == EOF || !print_html_escaped(out, &node->content) ||
            fputc(']', out) == EOF) {
          return 0;
        }
        return 1;
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
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_STYLE) {
        return render_style_advanced_block(out, node);
      }
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_SYNTAX) {
        return render_syntax_advanced_block(out, node);
      }
      if (node->advanced_type == NAMUMARK_NODE_ADVANCED_FOLDING) {
        return render_folding_advanced_block(out, node);
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

          int body_ok = 0;
          if (body.size >= 2 && body.ptr[0] == '#' && body.ptr[1] == '#') {
            body_ok = print_html_escaped(out, &body);
          } else {
            body_ok = render_advanced_content(out, &body);
          }
          if (!body_ok || fputs("</span>", out) < 0) {
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

static int parse_caption_line(const unsigned char *line, bufsize_t len, strbuf *caption,
                              strbuf *row) {
  bufsize_t s = 0;
  while (s < len && line[s] == ' ') {
    s++;
  }
  if (s + 2 >= len || line[s] != '|' || line[s + 1] == '|') {
    return 0;
  }

  bufsize_t cap_end = s + 1;
  while (cap_end < len && line[cap_end] != '|') {
    cap_end++;
  }
  if (cap_end >= len || cap_end + 2 >= len || line[cap_end + 1] != ' ' ||
      line[cap_end + 2] == '|') {
    return 0;
  }

  strbuf_clear(caption);
  strbuf_clear(row);
  strbuf_put(caption, line + s + 1, cap_end - (s + 1));
  strbuf_puts(row, "||");
  strbuf_put(row, line + cap_end + 1, len - (cap_end + 1));
  return 1;
}

static int is_table_line_start_for_render(const unsigned char *line, bufsize_t len) {
  strbuf caption;
  strbuf row;
  strbuf_init(&caption, 16);
  strbuf_init(&row, len + 2);
  int ok = is_row_start_line(line, len) || parse_caption_line(line, len, &caption, &row);
  strbuf_free(&caption);
  strbuf_free(&row);
  return ok;
}

typedef struct table_render_style {
  strbuf align;
  strbuf valign;
  strbuf width;
  strbuf height;
  strbuf bgcolor;
  strbuf color;
  strbuf class_name;
  int colspan;
  int rowspan;
  int no_padding;
  int thead;
  int sortable;
} table_render_style;

typedef struct table_render_context {
  strbuf width;
  strbuf bgcolor;
  strbuf bordercolor;
  strbuf align;
  strbuf col_bgcolor;
  strbuf col_color;
  strbuf class_name;
} table_render_context;

static void table_style_init(table_render_style *style) {
  strbuf_init(&style->align, 16);
  strbuf_init(&style->valign, 16);
  strbuf_init(&style->width, 32);
  strbuf_init(&style->height, 32);
  strbuf_init(&style->bgcolor, 32);
  strbuf_init(&style->color, 32);
  strbuf_init(&style->class_name, 32);
  style->colspan = 0;
  style->rowspan = 0;
  style->no_padding = 0;
  style->thead = 0;
  style->sortable = 0;
}

static void table_style_free(table_render_style *style) {
  strbuf_free(&style->align);
  strbuf_free(&style->valign);
  strbuf_free(&style->width);
  strbuf_free(&style->height);
  strbuf_free(&style->bgcolor);
  strbuf_free(&style->color);
  strbuf_free(&style->class_name);
}

static void table_context_init(table_render_context *ctx) {
  strbuf_init(&ctx->width, 32);
  strbuf_init(&ctx->bgcolor, 32);
  strbuf_init(&ctx->bordercolor, 32);
  strbuf_init(&ctx->align, 16);
  strbuf_init(&ctx->col_bgcolor, 32);
  strbuf_init(&ctx->col_color, 32);
  strbuf_init(&ctx->class_name, 32);
}

static void table_context_free(table_render_context *ctx) {
  strbuf_free(&ctx->width);
  strbuf_free(&ctx->bgcolor);
  strbuf_free(&ctx->bordercolor);
  strbuf_free(&ctx->align);
  strbuf_free(&ctx->col_bgcolor);
  strbuf_free(&ctx->col_color);
  strbuf_free(&ctx->class_name);
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

static int is_css_color_token(const unsigned char *value, bufsize_t len) {
  if (value == NULL || len <= 0) {
    return 0;
  }

  if (value[0] == '#') {
    if (len != 4 && len != 7) {
      return 0;
    }
    for (bufsize_t i = 1; i < len; i++) {
      if (!isxdigit((unsigned char)value[i])) {
        return 0;
      }
    }
    return 1;
  }

  static const char *aliases[] = {
    "black", "blue", "brown", "cyan", "gray", "green", "grey", "orange",
    "pink", "purple", "red", "tan", "transparent", "white", "yellow"
  };
  for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
    if (strlen(aliases[i]) == (size_t)len && memcmp(value, aliases[i], (size_t)len) == 0) {
      return 1;
    }
  }
  return 0;
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

  if (token[0] == '^' && token_len > 1 && token[1] == '|') {
    int span = parse_positive_int(token + 2, token_len - 2);
    if (span > 0) {
      cell_style->rowspan = span;
    }
    strbuf_set(&cell_style->valign, (const unsigned char *)"top", 3);
    return;
  }

  if (token[0] == 'v' && token_len > 1 && token[1] == '|') {
    int span = parse_positive_int(token + 2, token_len - 2);
    if (span > 0) {
      cell_style->rowspan = span;
    }
    strbuf_set(&cell_style->valign, (const unsigned char *)"bottom", 6);
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
  if (token_len == 5 && memcmp(token, "thead", 5) == 0) {
    row_style->thead = 1;
    return;
  }
  if (token_len == 8 && memcmp(token, "sortable", 8) == 0) {
    cell_style->sortable = 1;
    return;
  }

  if (is_css_color_token(token, token_len)) {
    strbuf_set(&cell_style->bgcolor, token, token_len);
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
  if (key_len == 6 && memcmp(token, "height", 6) == 0) {
    strbuf_set(&cell_style->height, val, val_len);
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
  if (key_len == 5 && memcmp(token, "class", 5) == 0) {
    strbuf_set(&cell_style->class_name, val, val_len);
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
  if (key_len == 8 && memcmp(token, "rowclass", 8) == 0) {
    strbuf_set(&row_style->class_name, val, val_len);
    return;
  }
  if (key_len == 8 && memcmp(token, "colcolor", 8) == 0) {
    set_primary_value(&table_style->col_color, val, val_len);
    return;
  }
  if (key_len == 10 && memcmp(token, "colbgcolor", 10) == 0) {
    set_primary_value(&table_style->col_bgcolor, val, val_len);
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
  if (key_len == 13 && memcmp(token, "table bgcolor", 13) == 0) {
    set_primary_value(&table_style->bgcolor, val, val_len);
    return;
  }
  if (key_len == 16 && memcmp(token, "tablebordercolor", 16) == 0) {
    set_primary_value(&table_style->bordercolor, val, val_len);
    return;
  }
  if (key_len == 17 && memcmp(token, "table bordercolor", 17) == 0) {
    set_primary_value(&table_style->bordercolor, val, val_len);
    return;
  }
  if (key_len == 10 && memcmp(token, "tablealign", 10) == 0) {
    strbuf_set(&table_style->align, val, val_len);
    return;
  }
  if (key_len == 10 && memcmp(token, "tableclass", 10) == 0) {
    strbuf_set(&table_style->class_name, val, val_len);
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

static bufsize_t find_token_in_range(const unsigned char *text, bufsize_t start,
                                     bufsize_t end, const char *token,
                                     bufsize_t token_len) {
  if (token_len == 0 || end < start || end - start < token_len) {
    return -1;
  }
  for (bufsize_t i = start; i + token_len <= end; i++) {
    if (memcmp(text + i, token, (size_t)token_len) == 0) {
      return i;
    }
  }
  return -1;
}

static bufsize_t find_wiki_advanced_start(const unsigned char *text, bufsize_t start,
                                          bufsize_t end) {
  if (end <= start) {
    return -1;
  }

  for (bufsize_t i = start; i + 9 <= end; i++) {
    int matched = 0;
    if (i + 9 <= end && memcmp(text + i, "{{{#!wiki", 9) == 0) {
      matched = 1;
    } else if (i + 12 <= end && memcmp(text + i, "{{{#!folding", 12) == 0) {
      matched = 1;
    } else if (i + 10 <= end && memcmp(text + i, "{{{#!style", 10) == 0) {
      matched = 1;
    }
    if (matched) {
      if (i > start && text[i - 1] == '{') {
        continue;
      }
      return i;
    }
  }

  return -1;
}

static int is_table_cell_literal_advanced(const unsigned char *text, bufsize_t start,
                                          bufsize_t end) {
  if (start + 3 > end || memcmp(text + start, "{{{", 3) != 0) {
    return 0;
  }
  if (start + 3 >= end) {
    return 1;
  }
  unsigned char c = text[start + 3];
  if (c == '#' || c == '+' || c == '-') {
    return 0;
  }
  return 1;
}

static bufsize_t find_table_cell_literal_advanced_start(const unsigned char *text,
                                                        bufsize_t start,
                                                        bufsize_t end) {
  for (bufsize_t i = start; i + 3 <= end; i++) {
    if (memcmp(text + i, "{{{", 3) != 0) {
      continue;
    }
    if (i > start && text[i - 1] == '{') {
      continue;
    }
    if (!(i + 5 <= end && text[i + 3] == '|' && text[i + 4] == '|')) {
      continue;
    }
    if (i + 3 >= end) {
      return i;
    }
    unsigned char c = text[i + 3];
    if (c != '#' && c != '+' && c != '-') {
      return i;
    }
  }
  return -1;
}

static int starts_folding_advanced_at(const unsigned char *text, bufsize_t pos, bufsize_t end) {
  return pos + 12 <= end && memcmp(text + pos, "{{{#!folding", 12) == 0;
}

static int is_folding_wrapper_prefix(const unsigned char *text, bufsize_t start, bufsize_t folding_start) {
  if (folding_start <= start) {
    return 0;
  }
  bufsize_t end = folding_start;
  while (end > start && text[end - 1] == ' ') {
    end--;
  }
  if (end == start + 3 && memcmp(text + start, "'''", 3) == 0) {
    return 1;
  }
  if (end > start + 4 && memcmp(text + start, "{{{#", 4) == 0) {
    return 1;
  }
  return 0;
}

static int parse_blockquote_line(const unsigned char *text, bufsize_t line_start,
                                 bufsize_t line_end, bufsize_t *content_start,
                                 bufsize_t *content_end, int *depth) {
  bufsize_t i = line_start;
  int count = 0;
  while (i < line_end && text[i] == '>') {
    i++;
    count++;
  }
  if (count == 0) {
    return 0;
  }
  if (i < line_end && text[i] == ' ') {
    i++;
  }
  *depth = count;
  *content_start = i;
  *content_end = line_end;
  while (*content_end > *content_start && text[*content_end - 1] == '\r') {
    (*content_end)--;
  }
  return 1;
}

static int render_blockquote_body(FILE *out, const unsigned char *text, bufsize_t len) {
  if (fputs("<blockquote><div>", out) < 0) {
    return 0;
  }
  if (!render_table_cell_content(out, text, len)) {
    return 0;
  }
  return fputs("</div></blockquote>", out) >= 0;
}

static int render_table_cell_content(FILE *out, const unsigned char *text, bufsize_t len) {
  if (len <= 0) {
    return 1;
  }

  strbuf wrapped_literal;
  strbuf_init(&wrapped_literal, len + 1);
  strbuf_set(&wrapped_literal, text, len);
  int wrapped_ok = render_wrapped_literal_body(out, &wrapped_literal);
  strbuf_free(&wrapped_literal);
  if (wrapped_ok) {
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
  const char *in_list_tag = "ul";
  int in_wiki_advanced = 0;
  int wiki_advanced_depth = 0;
  strbuf wiki_advanced;
  strbuf_init(&wiki_advanced, 128);
  int in_literal_block = 0;
  int literal_block_depth = 0;
  strbuf literal_block;
  strbuf_init(&literal_block, 128);
  int in_blockquote = 0;
  int blockquote_wiki_depth = 0;
  strbuf blockquote_body;
  strbuf_init(&blockquote_body, 128);

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

    if (in_literal_block) {
      bufsize_t close = -1;
      bufsize_t scan = line_start;
      while (scan + 2 < e) {
        if (text[scan] == '{' && text[scan + 1] == '{' && text[scan + 2] == '{') {
          literal_block_depth++;
          scan += 3;
          continue;
        }
        if (text[scan] == '}' && text[scan + 1] == '}' && text[scan + 2] == '}') {
          literal_block_depth--;
          if (literal_block_depth <= 0) {
            close = scan;
            break;
          }
          scan += 3;
          continue;
        }
        scan++;
      }
      bufsize_t take_end = close >= 0 ? close : e;
      if (literal_block.size > 0) {
        strbuf_putc(&literal_block, '\n');
      }
      if (take_end > line_start) {
        strbuf_put(&literal_block, text + line_start, take_end - line_start);
      }
      if (close >= 0) {
        if (fputs("<pre><code>", out) < 0 || !render_html_entity_or_text(out, literal_block.ptr, literal_block.size) ||
            fputs("</code></pre>", out) < 0) {
          strbuf_free(&literal_block);
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        strbuf_clear(&literal_block);
        in_literal_block = 0;
        literal_block_depth = 0;
        first_plain = 0;
        if (close + 3 < e) {
          line_start = close + 3;
          continue;
        }
      }

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
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
          strbuf_free(&blockquote_body);
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

    if (line_start + 1 < e && text[line_start] == '#' && text[line_start + 1] == '#') {
      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    if (is_table_cell_literal_advanced(text, s, e) &&
        !(s + 9 <= e && memcmp(text + s, "{{{#!wiki", 9) == 0)) {
      bufsize_t close = find_token_in_range(text, s + 3, e, "}}}", 3);
      if (!first_plain && fputs("<br />", out) < 0) {
        strbuf_free(&literal_block);
        strbuf_free(&blockquote_body);
        strbuf_free(&wiki_advanced);
        return 0;
      }
      if (close >= 0) {
        if (fputs("<code>", out) < 0 || !print_html_escaped_bytes(out, text + s + 3, close - (s + 3)) ||
            fputs("</code>", out) < 0) {
          strbuf_free(&literal_block);
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        first_plain = 0;
        if (close + 3 < e) {
          line_start = close + 3;
          continue;
        }
      } else {
        strbuf_clear(&literal_block);
        if (s + 3 < e) {
          strbuf_put(&literal_block, text + s + 3, e - (s + 3));
        }
        in_literal_block = 1;
        literal_block_depth = 1;
        literal_block_depth += count_token_occurrences(text + s + 3, e - (s + 3), "{{{", 3);
        literal_block_depth -= count_token_occurrences(text + s + 3, e - (s + 3), "}}}", 3);
        if (literal_block_depth <= 0) {
          literal_block_depth = 1;
        }
        first_plain = 0;
      }

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    bufsize_t literal_start = find_table_cell_literal_advanced_start(text, s, e);
    if (literal_start >= 0 && find_token_in_range(text, literal_start + 3, e, "}}}", 3) < 0) {
      if (literal_start > s) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&literal_block);
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        if (!render_inline_snippet(out, text + s, literal_start - s)) {
          strbuf_free(&literal_block);
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        first_plain = 0;
      } else if (!first_plain && fputs("<br />", out) < 0) {
        strbuf_free(&literal_block);
        strbuf_free(&blockquote_body);
        strbuf_free(&wiki_advanced);
        return 0;
      }

      strbuf_clear(&literal_block);
      if (literal_start + 3 < e) {
        strbuf_put(&literal_block, text + literal_start + 3, e - (literal_start + 3));
      }
      in_literal_block = 1;
      literal_block_depth = 1;
      literal_block_depth += count_token_occurrences(text + literal_start + 3,
                                                     e - (literal_start + 3), "{{{", 3);
      literal_block_depth -= count_token_occurrences(text + literal_start + 3,
                                                     e - (literal_start + 3), "}}}", 3);
      if (literal_block_depth <= 0) {
        literal_block_depth = 1;
      }
      first_plain = 0;

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    if (in_blockquote && blockquote_wiki_depth > 0) {
      if (blockquote_body.size > 0) {
        strbuf_putc(&blockquote_body, '\n');
      }
      if (e > line_start) {
        strbuf_put(&blockquote_body, text + line_start, e - line_start);
        blockquote_wiki_depth += count_token_occurrences(text + line_start,
                                                          e - line_start, "{{{", 3);
        blockquote_wiki_depth -= count_token_occurrences(text + line_start,
                                                          e - line_start, "}}}", 3);
        if (blockquote_wiki_depth < 0) {
          blockquote_wiki_depth = 0;
        }
      }

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    bufsize_t quote_start = 0;
    bufsize_t quote_end = 0;
    int quote_depth = 0;
    if (parse_blockquote_line(text, s, e, &quote_start, &quote_end, &quote_depth)) {
      (void)quote_depth;
      while (in_list > 0) {
        if (fprintf(out, "</li></%s>", in_list_tag) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }
      if (!in_blockquote) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_blockquote = 1;
      } else if (blockquote_body.size > 0) {
        strbuf_putc(&blockquote_body, '\n');
      }
      if (quote_end > quote_start) {
        strbuf_put(&blockquote_body, text + quote_start, quote_end - quote_start);
        blockquote_wiki_depth += count_token_occurrences(text + quote_start,
                                                          quote_end - quote_start, "{{{", 3);
        blockquote_wiki_depth -= count_token_occurrences(text + quote_start,
                                                          quote_end - quote_start, "}}}", 3);
        if (blockquote_wiki_depth < 0) {
          blockquote_wiki_depth = 0;
        }
      }
      first_plain = 0;

      if (line_end >= len) {
        break;
      }
      line_start = line_end + 1;
      continue;
    }

    if (in_blockquote) {
      if (!render_blockquote_body(out, blockquote_body.ptr, blockquote_body.size)) {
        strbuf_free(&blockquote_body);
        strbuf_free(&wiki_advanced);
        return 0;
      }
      strbuf_clear(&blockquote_body);
      in_blockquote = 0;
      blockquote_wiki_depth = 0;
      first_plain = 0;
    }

    bufsize_t wiki_start = (e > s) ? find_wiki_advanced_start(text, s, e) : -1;
    if (wiki_start >= 0) {
      if (in_blockquote) {
        if (!render_blockquote_body(out, blockquote_body.ptr, blockquote_body.size)) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        strbuf_clear(&blockquote_body);
        in_blockquote = 0;
        blockquote_wiki_depth = 0;
        first_plain = 0;
      }
      if (in_list) {
        if (fprintf(out, "</li></%s>", in_list_tag) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }

      bufsize_t collect_start = wiki_start;
      if (starts_folding_advanced_at(text, wiki_start, e) && is_folding_wrapper_prefix(text, s, wiki_start)) {
        collect_start = s;
      }

      if (collect_start > s) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        if (!render_inline_snippet(out, text + s, collect_start - s)) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        first_plain = 0;
      }

      if (wiki_advanced.size > 0) {
        strbuf_putc(&wiki_advanced, '\n');
      }
      strbuf_put(&wiki_advanced, text + collect_start, e - collect_start);

      int starts = count_token_occurrences(text + collect_start, e - collect_start, "{{{", 3);
      int ends = count_token_occurrences(text + collect_start, e - collect_start, "}}}", 3);
      wiki_advanced_depth = starts - ends;

      if (wiki_advanced_depth <= 0) {
        if (!render_inline_snippet(out, wiki_advanced.ptr, wiki_advanced.size)) {
          strbuf_free(&blockquote_body);
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
    int is_ordered_list = 0;
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
      } else if (leading + 1 < line_end && isdigit(text[leading]) && text[leading + 1] == '.' &&
                 (leading + 2 >= line_end || text[leading + 2] == ' ')) {
        is_list = 1;
        is_ordered_list = 1;
        list_start = leading + 2;
        if (list_start < line_end && text[list_start] == ' ') {
          list_start++;
        }
      }
    }

    if (is_list) {
      while (in_list > list_indent) {
        if (fprintf(out, "</li></%s>", in_list_tag) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }
      if (in_list == 0) {
        in_list_tag = is_ordered_list ? "ol" : "ul";
        if (fprintf(out, "<%s>", in_list_tag) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list = 1;
        if (fputs("<li>", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
      } else {
        while (in_list < list_indent) {
          if (fprintf(out, "<%s><li>", in_list_tag) < 0) {
            strbuf_free(&blockquote_body);
            strbuf_free(&wiki_advanced);
            return 0;
          }
          in_list++;
        }
        if (fputs("</li>\n<li>", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
      }
      if (!render_inline_snippet(out, text + list_start, e - list_start)) {
        strbuf_free(&blockquote_body);
        strbuf_free(&wiki_advanced);
        return 0;
      }
    } else {
      while (in_list > 0) {
        if (fprintf(out, "</li></%s>", in_list_tag) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        in_list--;
      }

      if (is_horizontal_rule_text(text + s, e - s)) {
        if (fputs("<hr>", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
      } else if (e > line_start) {
        if (!first_plain && fputs("<br />", out) < 0) {
          strbuf_free(&blockquote_body);
          strbuf_free(&wiki_advanced);
          return 0;
        }
        if (!render_inline_snippet(out, text + line_start, e - line_start)) {
          strbuf_free(&blockquote_body);
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
    if (fprintf(out, "</li></%s>", in_list_tag) < 0) {
      strbuf_free(&blockquote_body);
      strbuf_free(&wiki_advanced);
      return 0;
    }
    in_list--;
  }

  if (in_blockquote) {
    if (!render_blockquote_body(out, blockquote_body.ptr, blockquote_body.size)) {
      strbuf_free(&blockquote_body);
      strbuf_free(&wiki_advanced);
      return 0;
    }
  }

  if (wiki_advanced.size > 0) {
    if (!render_inline_snippet(out, wiki_advanced.ptr, wiki_advanced.size)) {
      strbuf_free(&blockquote_body);
      strbuf_free(&wiki_advanced);
      return 0;
    }
  }

  strbuf_free(&blockquote_body);
  strbuf_free(&wiki_advanced);

  return 1;
}

static int render_table_row(FILE *out, const strbuf *row, int row_index,
                            table_render_context *table_style, int *thead_open,
                            int *tbody_open) {
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

  table_render_style row_style;
  table_style_init(&row_style);

  {
    bufsize_t probe = s + 2;
    while (probe < line_end) {
      bufsize_t probe_sep = find_table_cell_separator(row->ptr, probe, line_end);
      bufsize_t probe_end = (probe_sep >= 0) ? probe_sep : line_end;
      strbuf probe_cell;
      strbuf_init(&probe_cell, probe_end - probe + 1);
      if (probe_end > probe) {
        strbuf_set(&probe_cell, row->ptr + probe, probe_end - probe);
      }
      table_render_style probe_cell_style;
      table_style_init(&probe_cell_style);
      bufsize_t probe_content_start = 0;
      parse_cell_prefix(&probe_cell, table_style, &row_style, &probe_cell_style, &probe_content_start);
      table_style_free(&probe_cell_style);
      strbuf_free(&probe_cell);
      if (probe_sep < 0 || probe_sep + 1 >= line_end) {
        break;
      }
      probe = probe_sep + 2;
    }
  }

  if (row_style.thead && thead_open != NULL && !*thead_open) {
    if (fputs("<thead>", out) < 0) {
      table_style_free(&row_style);
      return 0;
    }
    *thead_open = 1;
  } else if (!row_style.thead && thead_open != NULL && *thead_open) {
    if (fputs("</thead><tbody>\n", out) < 0) {
      table_style_free(&row_style);
      return 0;
    }
    *thead_open = 0;
    if (tbody_open != NULL) {
      *tbody_open = 1;
    }
  }

  if (fputs("<tr", out) < 0) {
    table_style_free(&row_style);
    return 0;
  }
  if (row_style.class_name.size > 0) {
    if (fputs(" class=\"", out) < 0 || !print_html_escaped(out, &row_style.class_name) ||
        fputs("\"", out) < 0) {
      table_style_free(&row_style);
      return 0;
    }
  }
  if (fputc('>', out) == EOF) {
    table_style_free(&row_style);
    return 0;
  }

  bufsize_t p = s + 2;
  int cell_index = 0;
  while (p < line_end) {
    int implicit_colspan = 1;
    while (p + 1 < line_end && row->ptr[p] == '|' && row->ptr[p + 1] == '|') {
      implicit_colspan++;
      p += 2;
    }

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
    cell_style.colspan = implicit_colspan;
    bufsize_t content_start = 0;
    parse_cell_prefix(&cell, table_style, &row_style, &cell_style, &content_start);

    bufsize_t content_end = cell.size;
    trim_spaces(&cell, &content_start, &content_end);

    if (implicit_colspan > 1 && content_start >= content_end) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      if (sep < 0 || sep + 1 >= line_end) {
        break;
      }
      cell_index += implicit_colspan;
      p = sep + 2;
      continue;
    }

    const char *tag = row_style.thead ? "th" : "td";
    if (fputs("<", out) < 0 || fputs(tag, out) < 0) {
      table_style_free(&cell_style);
      strbuf_free(&cell);
      table_style_free(&row_style);
      return 0;
    }

    if (cell_style.sortable || cell_style.class_name.size > 0) {
      if (fputs(" class=\"", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
      if (cell_style.sortable && fputs("nm-sortable", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
      if (cell_style.class_name.size > 0) {
        if (cell_style.sortable && fputc(' ', out) == EOF) {
          table_style_free(&cell_style);
          strbuf_free(&cell);
          table_style_free(&row_style);
          return 0;
        }
        if (!print_html_escaped(out, &cell_style.class_name)) {
          table_style_free(&cell_style);
          strbuf_free(&cell);
          table_style_free(&row_style);
          return 0;
        }
      }
      if (fputc('"', out) == EOF) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
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
    if (cell_style.height.size > 0) {
      if (fputs("height:", out) < 0 || !print_html_escaped(out, &cell_style.height) ||
          fputs("px;", out) < 0) {
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

    if (cell_style.valign.size > 0) {
      if (fputs("vertical-align:", out) < 0 || !print_html_escaped(out, &cell_style.valign) ||
          fputs(";", out) < 0) {
        table_style_free(&cell_style);
        strbuf_free(&cell);
        table_style_free(&row_style);
        return 0;
      }
    }

    const strbuf *bgcolor = (cell_style.bgcolor.size > 0) ? &cell_style.bgcolor :
                            (row_style.bgcolor.size > 0) ? &row_style.bgcolor : NULL;
    if (bgcolor == NULL && cell_index == 0 && table_style->col_bgcolor.size > 0) {
      bgcolor = &table_style->col_bgcolor;
    }
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
    if (color == NULL && cell_index == 0 && table_style->col_color.size > 0) {
      color = &table_style->col_color;
    }
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
    cell_index++;
    p = sep + 2;
  }

  if (fputs("</tr>\n", out) < 0) {
    table_style_free(&row_style);
    return 0;
  }
  table_style_free(&row_style);
  return 1;
}

static int table_row_is_thead(const strbuf *row) {
  if (row == NULL || row->size == 0) {
    return 0;
  }
  bufsize_t line_end = row->size;
  while (line_end > 0 && (row->ptr[line_end - 1] == '\n' || row->ptr[line_end - 1] == '\r' ||
                          row->ptr[line_end - 1] == ' ')) {
    line_end--;
  }
  bufsize_t p = 0;
  while (p < line_end && row->ptr[p] == ' ') {
    p++;
  }
  if (p + 1 >= line_end || row->ptr[p] != '|' || row->ptr[p + 1] != '|') {
    return 0;
  }
  p += 2;
  while (p < line_end) {
    bufsize_t sep = find_table_cell_separator(row->ptr, p, line_end);
    bufsize_t cell_end = sep >= 0 ? sep : line_end;
    bufsize_t cell_start = p;
    while (cell_start < cell_end && row->ptr[cell_start] == ' ') {
      cell_start++;
    }
    if (cell_start + 7 <= cell_end && memcmp(row->ptr + cell_start, "<thead>", 7) == 0) {
      return 1;
    }
    if (sep < 0 || sep + 1 >= line_end) {
      break;
    }
    p = sep + 2;
  }
  return 0;
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
          if (continuation->type == NAMUMARK_NODE_TABLE || continuation->type == NAMUMARK_NODE_WIKI_BLOCK) {
            namumark_node block_copy = *continuation;
            block_copy.first_child = NULL;
            block_copy.last_child = NULL;
            block_copy.next = NULL;
            block_copy.prev = NULL;
            strbuf_init(&block_copy.content, continuation->content.size - start + 1);
            if (continuation->type == NAMUMARK_NODE_TABLE && start < continuation->content.size) {
              strbuf_put(&block_copy.content, continuation->content.ptr + start,
                         continuation->content.size - start);
            } else {
              strbuf_set(&block_copy.content, continuation->content.ptr, continuation->content.size);
            }
            int ok = render_block_node(out, &block_copy);
            strbuf_free(&block_copy.content);
            if (!ok) {
              return 0;
            }
            continue;
          }
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
    {
      strbuf body;
      strbuf_init(&body, node->content.size + 1);

      const namumark_node *quote = node;
      while (quote != NULL && is_blockquote_sequence_node(quote)) {
        if (body.size > 0) {
          strbuf_putc(&body, '\n');
        }
        if (quote->content.size > 0) {
          strbuf_put(&body, quote->content.ptr, quote->content.size);
        }
        quote = quote->next;
      }

      int ok = render_blockquote_body(out, body.ptr, body.size) && fputs("\n", out) >= 0;
      strbuf_free(&body);
      if (!ok) {
        return 0;
      }
      return 1;
    }
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
            if (is_table_line_start_for_render(line_ptr, line_len)) {
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

      if (fputs("<table class=\"nm-table", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }
      if (table_style.class_name.size > 0) {
        if (fputc(' ', out) == EOF || !print_html_escaped(out, &table_style.class_name)) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }
      if (fputc('"', out) == EOF) {
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

      if (fputs("\">", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }

      int tbody_open = 0;
      int thead_open = 0;
      int caption_rendered = 0;

      while (line_start <= tablebuf.size) {
        bufsize_t line_end = strbuf_strchr(&tablebuf, '\n', line_start);
        if (line_end < 0) {
          line_end = tablebuf.size;
        }

        if (line_end > line_start) {
          const unsigned char *line_ptr = tablebuf.ptr + line_start;
          bufsize_t line_len = line_end - line_start;

          if (is_table_line_start_for_render(line_ptr, line_len)) {
            if (pending_row.size > 0 && table_row_ends_with_separator(&pending_row)) {
              if (!thead_open && !tbody_open && !table_row_is_thead(&pending_row)) {
                if (fputs("<tbody>\n", out) < 0) {
                  strbuf_free(&pending_row);
                  strbuf_free(&tablebuf);
                  table_context_free(&table_style);
                  return 0;
                }
                tbody_open = 1;
              }
              if (!render_table_row(out, &pending_row, row_index, &table_style, &thead_open,
                                    &tbody_open)) {
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
              strbuf caption;
              strbuf caption_row;
              strbuf_init(&caption, 32);
              strbuf_init(&caption_row, line_len + 4);
              if (!caption_rendered && parse_caption_line(line_ptr, line_len, &caption, &caption_row)) {
                if (fputs("<caption>", out) < 0 ||
                    !render_table_cell_content(out, caption.ptr, caption.size) ||
                    fputs("</caption>", out) < 0) {
                  strbuf_free(&caption);
                  strbuf_free(&caption_row);
                  strbuf_free(&pending_row);
                  strbuf_free(&tablebuf);
                  table_context_free(&table_style);
                  return 0;
                }
                strbuf_set(&pending_row, caption_row.ptr, caption_row.size);
                caption_rendered = 1;
              } else {
                strbuf_set(&pending_row, line_ptr, line_len);
              }
              strbuf_free(&caption);
              strbuf_free(&caption_row);
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
        if (!thead_open && !tbody_open && !table_row_is_thead(&pending_row)) {
          if (fputs("<tbody>\n", out) < 0) {
            strbuf_free(&pending_row);
            strbuf_free(&tablebuf);
            table_context_free(&table_style);
            return 0;
          }
          tbody_open = 1;
        }
        if (!render_table_row(out, &pending_row, row_index, &table_style, &thead_open,
                              &tbody_open)) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }

      if (thead_open && fputs("</thead><tbody>\n", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }
      if (thead_open) {
        tbody_open = 1;
      }

      if (!tbody_open) {
        if (fputs("<tbody>\n", out) < 0) {
          strbuf_free(&pending_row);
          strbuf_free(&tablebuf);
          table_context_free(&table_style);
          return 0;
        }
      }

      if (fputs("</tbody></table>\n", out) < 0) {
        strbuf_free(&pending_row);
        strbuf_free(&tablebuf);
        table_context_free(&table_style);
        return 0;
      }
      strbuf_free(&pending_row);
      strbuf_free(&tablebuf);
      table_context_free(&table_style);
      return 1;
    }
    case NAMUMARK_NODE_WIKI_BLOCK: {
      strbuf wikibuf;
      strbuf_init(&wikibuf, node->content.size + 1);
      strbuf_set(&wikibuf, node->content.ptr, node->content.size);
      const char *tag = wiki_render_tag(node, 0);

      if (!wiki_body_needs_block_rendering(&wikibuf)) {
      if (!render_wiki_block_open(out, tag, node)) {
        strbuf_free(&wikibuf);
        return 0;
      }
      int ok = 0;
      if (wikibuf.size >= 6 && memcmp(wikibuf.ptr, "{{{", 3) == 0) {
        ok = render_wrapped_literal_body(out, &wikibuf);
      }
      if (!ok && !render_advanced_content(out, &wikibuf)) {
        strbuf_free(&wikibuf);
        return 0;
      }
        strbuf_free(&wikibuf);
        if (fprintf(out, "</%s>", tag) < 0) {
          return 0;
        }
        if (style_has_inline_display(&node->args)) {
          return fputs("<br />\n", out) >= 0;
        }
        return fputc('\n', out) != EOF;
      }

      if (!render_wiki_block_open(out, tag, node) || fputc('\n', out) == EOF) {
        strbuf_free(&wikibuf);
        return 0;
      }

      /* Leading #!style blocks inside a #!wiki body are global style blocks, not paragraphs. */
      if (!render_leading_style_blocks(out, &wikibuf)) {
        strbuf_free(&wikibuf);
        return 0;
      }

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

      if (!render_block_children(out, subdoc)) {
        namumark_node_free(subdoc);
        return 0;
      }

      namumark_node_free(subdoc);
      if (fprintf(out, "</%s>\n", tag) < 0) {
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
      if (active_footnotes != NULL) {
        return footnote_get_or_add_entry(active_footnotes, node->label.ptr, node->label.size,
                                         node->content.ptr, node->content.size) != NULL;
      } else {
        if (fputs("<div class=\"nm-footnote\" data-label=\"", out) < 0 ||
            !print_html_escaped(out, &node->label) || fputs("\">", out) < 0 ||
            !render_inline_children(out, node) || fputs("</div>\n", out) < 0) {
          return 0;
        }
      }
      return 1;
    case NAMUMARK_NODE_TEXT:
      if (is_footnote_macro_only_text(node) || is_style_advanced_only_text(node)) {
        return render_inline_children(out, node);
      }
      if (text_contains_block_macro(node)) {
        return render_text_with_block_macro(out, node);
      }
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
    if (child->type == NAMUMARK_NODE_BLOCKQUOTE) {
      while (child->next != NULL && is_blockquote_sequence_node(child->next)) {
        child = child->next;
      }
    }
    child = child->next;
  }
  return 1;
}

static int render_onclick_runtime_script(FILE *out) {
  static const char script[] =
      "<script>(function(){\n"
      "function validClassName(value){return /^[A-Za-z0-9_-]+$/.test(value);}\n"
      "function applyAction(root, action){\n"
      "var parts=action.split(',');\n"
      "if(parts.length!==3)return;\n"
      "var op=parts[0].trim();\n"
      "var target=parts[1].trim();\n"
      "var className=parts[2].trim();\n"
      "if(!validClassName(target)||!validClassName(className))return;\n"
      "if(op!=='add-class'&&op!=='remove-class'&&op!=='toggle-class')return;\n"
      "var nodes=root.getElementsByClassName(target);\n"
      "var list=Array.prototype.slice.call(nodes);\n"
      "for(var i=0;i<list.length;i++){\n"
      "if(op==='add-class')list[i].classList.add(className);\n"
      "else if(op==='remove-class')list[i].classList.remove(className);\n"
      "else list[i].classList.toggle(className);\n"
      "}\n"
      "}\n"
      "document.addEventListener('click',function(event){\n"
      "var trigger=event.target.closest('[data-onclick]');\n"
      "if(!trigger)return;\n"
      "var root=trigger.closest('article.namumark')||document;\n"
      "var actions=trigger.getAttribute('data-onclick').split(';');\n"
      "for(var i=0;i<actions.length;i++)applyAction(root,actions[i]);\n"
      "});\n"
      "})();</script>\n";
  return fputs(script, out) >= 0;
}

int print_document_html(const namumark_node *document, FILE *out) {
  if (document == NULL || out == NULL) {
    return 0;
  }

  footnote_render_context footnotes;
  footnote_context_init(&footnotes);
  footnote_render_context *previous_footnotes = active_footnotes;
  active_footnotes = &footnotes;

  if (fputs("<article class=\"namumark\">\n", out) < 0) {
    active_footnotes = previous_footnotes;
    footnote_context_free(&footnotes);
    return 0;
  }

  if (!render_block_children(out, document)) {
    active_footnotes = previous_footnotes;
    footnote_context_free(&footnotes);
    return 0;
  }

  if (!render_footnote_list(out) || !render_onclick_runtime_script(out) ||
      fputs("</article>\n", out) < 0) {
    active_footnotes = previous_footnotes;
    footnote_context_free(&footnotes);
    return 0;
  }

  active_footnotes = previous_footnotes;
  footnote_context_free(&footnotes);
  return 1;
}

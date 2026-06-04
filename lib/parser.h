/**
 * @file parser.h
 * @brief Streaming block parser state and lifecycle functions.
 *
 * The parser accepts arbitrarily chunked input through parser_feed().  It keeps
 * block-level continuation state here because NamuMark tables and {{{#!wiki}}}
 * blocks can legally span many physical lines and can contain nested table-like
 * text that must not become a new outer block.
 */
#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdbool.h>

#include "node.h"
#include "strbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct namumark_parser {
  /** Root document node receiving all parsed block children. */
  struct namumark_node *root;
  /** Current open block used by simple continuation paths. */
  struct namumark_node *current;

  /** 1-based logical line number after line ending normalization. */
  int line_number;
  /** Total byte offset consumed from all feeds. */
  bufsize_t offset;
  /** Length of the last normalized line, used for end-column metadata. */
  bufsize_t last_line_length;

  /** Total input byte count, retained for diagnostics and future limits. */
  size_t total_size;

  /** Previous feed ended with CR, so parser_feed() must merge a following LF. */
  bool last_buffer_endded_with_cr;
  /** Redirect lines ignore the rest of the document by NamuMark convention. */
  bool ignore_remaining_lines;
  /** True while the last table row can accept continuation lines. */
  bool table_continuation;
  /** Blank lines split adjacent tables unless nested syntax is still open. */
  bool table_interrupted_by_blank;
  /** Open {{{#!wiki}}} count while collecting a table row. */
  int table_wiki_block_depth;
  /** Open non-wiki {{{...}}} count while collecting a table row. */
  int table_wiki_nonwiki_depth;
  /** Open top-level block {{{#!wiki}}} count. */
  int wiki_block_depth;
  /** Open non-wiki advanced blocks inside the current {{{#!wiki}}}. */
  int wiki_nonwiki_depth;
  /** Generic advanced brace depth for block-level preformatted transitions. */
  int advanced_brace_depth;
  /** Inline advanced depth used while processing same-line block transitions. */
  int inline_advanced_depth;

  /** Node receiving content for an open block {{{#!wiki}}}. */
  struct namumark_node *wiki_block_node;
  /** Node receiving content for block-level advanced/preformatted text. */
  struct namumark_node *advanced_text_node;
  /** Node receiving ordinary text until inline parsing runs. */
  struct namumark_node *inline_text_node;

  /** Current physical line accumulated across parser_feed() calls. */
  strbuf current_line;
} namumark_parser;

/** @brief Reset a parser to a fresh document while preserving the allocation. */
void parser_reset(namumark_parser *parser);
/** @return A new parser or NULL on allocation failure. */
namumark_parser *parser_new(void);
/** @brief Free a parser and all nodes owned by its unfinished document. */
void parser_free(namumark_parser *parser);
/** @brief Feed arbitrary input bytes into the streaming parser. */
void parser_feed(namumark_parser *parser, const unsigned char *buffer, size_t len);
/** @brief Finish parsing and transfer document ownership to the caller. */
namumark_node *parser_finish(namumark_parser *parser);

#ifdef __cplusplus
}
#endif

#endif // __PARSER_H__

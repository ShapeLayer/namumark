#include <stdlib.h>

#include "namumark.h"
#include "node.h"
#include "parser.h"
#include "renderer.h"

const char *namumark_version(void) {
  return NAMUMARK_VERSION;
}

const char *namumark_status_message(namumark_status status) {
  switch (status) {
    case NAMUMARK_OK:
      return "ok";
    case NAMUMARK_ERROR_INVALID_ARGUMENT:
      return "invalid argument";
    case NAMUMARK_ERROR_ALLOCATION:
      return "allocation failed";
    case NAMUMARK_ERROR_PARSE:
      return "parse failed";
    case NAMUMARK_ERROR_RENDER:
      return "render failed";
    default:
      return "unknown error";
  }
}

static namumark_status render_document_to_stream(namumark_node *document,
                                                 namumark_output_format format,
                                                 FILE *stream) {
  int ok = 0;
  switch (format) {
    case NAMUMARK_OUTPUT_HTML:
      ok = print_document_html(document, stream);
      break;
    case NAMUMARK_OUTPUT_AST_JSON:
      ok = print_document_ast(document, stream);
      break;
    default:
      return NAMUMARK_ERROR_INVALID_ARGUMENT;
  }
  return ok ? NAMUMARK_OK : NAMUMARK_ERROR_RENDER;
}

namumark_status namumark_render(const char *input, size_t input_size,
                                namumark_output_format format,
                                namumark_buffer *output) {
  /*
   * The public API resets output before doing any work so FFI callers can safely
   * call namumark_buffer_free() only after NAMUMARK_OK.  This convention keeps
   * bindings simple and prevents double-free ambiguity on error paths.
   */
  if (input == NULL || output == NULL) {
    return NAMUMARK_ERROR_INVALID_ARGUMENT;
  }

  output->data = NULL;
  output->size = 0;

  namumark_parser *parser = parser_new();
  if (parser == NULL) {
    return NAMUMARK_ERROR_ALLOCATION;
  }

  parser_feed(parser, (const unsigned char *)input, input_size);
  namumark_node *document = parser_finish(parser);
  parser_free(parser);
  if (document == NULL) {
    return NAMUMARK_ERROR_PARSE;
  }

  char *data = NULL;
  size_t size = 0;
  /*
   * open_memstream gives the renderers a FILE* without exposing FILE ownership
   * to embedders.  The resulting heap buffer becomes namumark_buffer::data on
   * success and is freed locally on render failure.
   */
  FILE *stream = open_memstream(&data, &size);
  if (stream == NULL) {
    namumark_node_free(document);
    return NAMUMARK_ERROR_ALLOCATION;
  }

  namumark_status status = render_document_to_stream(document, format, stream);
  if (fclose(stream) != 0 && status == NAMUMARK_OK) {
    status = NAMUMARK_ERROR_RENDER;
  }
  namumark_node_free(document);

  if (status != NAMUMARK_OK) {
    free(data);
    return status;
  }

  output->data = data;
  output->size = size;
  return NAMUMARK_OK;
}

namumark_status namumark_render_html(const char *input, size_t input_size,
                                     namumark_buffer *output) {
  return namumark_render(input, input_size, NAMUMARK_OUTPUT_HTML, output);
}

namumark_status namumark_render_ast_json(const char *input, size_t input_size,
                                         namumark_buffer *output) {
  return namumark_render(input, input_size, NAMUMARK_OUTPUT_AST_JSON, output);
}

void namumark_buffer_free(namumark_buffer *buffer) {
  if (buffer == NULL) {
    return;
  }
  free(buffer->data);
  buffer->data = NULL;
  buffer->size = 0;
}

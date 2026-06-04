#ifndef NAMUMARK_PUBLIC_H
#define NAMUMARK_PUBLIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAMUMARK_VERSION "0.0.1"

typedef enum namumark_output_format {
  NAMUMARK_OUTPUT_HTML = 0,
  NAMUMARK_OUTPUT_AST_JSON = 1,
} namumark_output_format;

typedef enum namumark_status {
  NAMUMARK_OK = 0,
  NAMUMARK_ERROR_INVALID_ARGUMENT = 1,
  NAMUMARK_ERROR_ALLOCATION = 2,
  NAMUMARK_ERROR_PARSE = 3,
  NAMUMARK_ERROR_RENDER = 4,
} namumark_status;

typedef struct namumark_buffer {
  char *data;
  size_t size;
} namumark_buffer;

const char *namumark_version(void);
const char *namumark_status_message(namumark_status status);

namumark_status namumark_render(const char *input, size_t input_size,
                                namumark_output_format format,
                                namumark_buffer *output);
namumark_status namumark_render_html(const char *input, size_t input_size,
                                     namumark_buffer *output);
namumark_status namumark_render_ast_json(const char *input, size_t input_size,
                                         namumark_buffer *output);
void namumark_buffer_free(namumark_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif // NAMUMARK_PUBLIC_H

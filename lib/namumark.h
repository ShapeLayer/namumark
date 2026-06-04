/**
 * @file namumark.h
 * @brief Stable C API for rendering NamuMark markup to HTML or AST JSON.
 *
 * The functions in this header are intentionally buffer-oriented so that FFI
 * bindings can call the library without depending on internal parser or AST
 * layouts.  Internal headers expose richer structures for tests and the CLI,
 * but embedders should treat this file as the compatibility boundary.
 */
#ifndef NAMUMARK_PUBLIC_H
#define NAMUMARK_PUBLIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Public API version string returned by namumark_version(). */
#define NAMUMARK_VERSION "0.0.1"

/**
 * @brief Output formats supported by the stable renderer entry point.
 *
 * The enum values are fixed because Python, Node.js, and Rust bindings pass
 * them through FFI as integers.
 */
typedef enum namumark_output_format {
  /** Render the document as HTML. */
  NAMUMARK_OUTPUT_HTML = 0,
  /** Render the parser tree as diagnostic JSON. */
  NAMUMARK_OUTPUT_AST_JSON = 1,
} namumark_output_format;

/**
 * @brief Status codes returned by public rendering functions.
 *
 * Non-zero values are deliberately coarse-grained.  The parser is permissive,
 * so most user input problems are represented in the output tree instead of as
 * hard failures.
 */
typedef enum namumark_status {
  /** Rendering completed and output owns a heap buffer. */
  NAMUMARK_OK = 0,
  /** A required pointer was NULL or an enum value was unknown. */
  NAMUMARK_ERROR_INVALID_ARGUMENT = 1,
  /** The library could not allocate parser or output memory. */
  NAMUMARK_ERROR_ALLOCATION = 2,
  /** Parser finalization failed unexpectedly. */
  NAMUMARK_ERROR_PARSE = 3,
  /** Renderer output failed, usually due to FILE stream failure. */
  NAMUMARK_ERROR_RENDER = 4,
} namumark_status;

/**
 * @brief Heap-owned render result returned through the C API.
 *
 * @param data NUL-terminated byte buffer allocated by the library.  The data is
 *             valid until namumark_buffer_free() is called.
 * @param size Number of meaningful bytes in data, excluding the convenience NUL.
 */
typedef struct namumark_buffer {
  char *data;
  size_t size;
} namumark_buffer;

/**
 * @return A static string describing the public C API version.
 */
const char *namumark_version(void);

/**
 * @param status A status code returned from this API.
 * @return A static English message for diagnostics and FFI exceptions.
 */
const char *namumark_status_message(namumark_status status);

/**
 * @brief Render a NamuMark byte buffer into a heap-owned output buffer.
 *
 * @param input Input bytes.  The buffer does not need to be NUL-terminated.
 * @param input_size Number of bytes in input.
 * @param format Desired renderer output format.
 * @param output Output buffer descriptor initialized by the function.
 * @return NAMUMARK_OK on success; a non-zero status otherwise.
 *
 * The output buffer is always reset before rendering.  Call
 * namumark_buffer_free() after a successful render.
 */
namumark_status namumark_render(const char *input, size_t input_size,
                                namumark_output_format format,
                                namumark_buffer *output);

/**
 * @brief Convenience wrapper for namumark_render(..., NAMUMARK_OUTPUT_HTML, ...).
 */
namumark_status namumark_render_html(const char *input, size_t input_size,
                                     namumark_buffer *output);

/**
 * @brief Convenience wrapper for namumark_render(..., NAMUMARK_OUTPUT_AST_JSON, ...).
 */
namumark_status namumark_render_ast_json(const char *input, size_t input_size,
                                         namumark_buffer *output);

/**
 * @brief Release and clear a buffer returned by a successful render call.
 *
 * Passing NULL is allowed.  Passing an already-cleared buffer is allowed.
 */
void namumark_buffer_free(namumark_buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif // NAMUMARK_PUBLIC_H

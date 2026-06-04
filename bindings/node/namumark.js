'use strict';

/*
 * Node.js FFI binding for the stable namumark C API.
 * Koffi is used instead of ffi-napi because it supports current Node releases
 * without compiling a project-specific native addon.  The C API still owns the
 * render buffer; JavaScript decodes it and frees it in the finally block.
 */

const koffi = require('koffi');
const path = require('path');
const fs = require('fs');

const NAMUMARK_OUTPUT_HTML = 0;
const NAMUMARK_OUTPUT_AST_JSON = 1;
const NAMUMARK_OK = 0;

koffi.struct('namumark_buffer', {
  data: 'void *',
  size: 'size_t',
});

function defaultLibraryPath() {
  /* Prefer the CMake build tree so examples work without installation. */
  const root = path.resolve(__dirname, '..', '..');
  const candidates = [
    path.join(root, 'build', 'libnamumark.dylib'),
    path.join(root, 'build', 'libnamumark.so'),
    path.join(root, 'build', 'namumark.dll'),
  ];
  return candidates.find((candidate) => fs.existsSync(candidate)) || candidates[0];
}

function loadLibrary(libraryPath) {
  const lib = koffi.load(libraryPath || process.env.NAMUMARK_LIBRARY || defaultLibraryPath());
  return {
    render: lib.func('int namumark_render(const char *input, size_t input_size, int format, _Out_ namumark_buffer *output)'),
    free: lib.func('void namumark_buffer_free(namumark_buffer *output)'),
    message: lib.func('const char *namumark_status_message(int status)'),
  };
}

class Namumark {
  constructor(libraryPath) {
    this.lib = loadLibrary(libraryPath);
  }

  render(input, outputFormat = NAMUMARK_OUTPUT_HTML) {
    /* Buffer.byteLength equals the C input size because the native API accepts bytes. */
    const data = Buffer.isBuffer(input) ? input : Buffer.from(String(input), 'utf8');
    const output = {};
    const status = this.lib.render(data, data.length, outputFormat, output);
    if (status !== NAMUMARK_OK) {
      throw new Error(this.lib.message(status));
    }
    try {
      return koffi.decode(output.data, 'char', Number(output.size));
    } finally {
      this.lib.free(output);
    }
  }

  renderHtml(input) {
    return this.render(input, NAMUMARK_OUTPUT_HTML);
  }

  renderAstJson(input) {
    return this.render(input, NAMUMARK_OUTPUT_AST_JSON);
  }
}

const defaultClient = new Namumark();

module.exports = {
  NAMUMARK_OUTPUT_HTML,
  NAMUMARK_OUTPUT_AST_JSON,
  Namumark,
  renderHtml: (input) => defaultClient.renderHtml(input),
  renderAstJson: (input) => defaultClient.renderAstJson(input),
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/parser.h"
#include "lib/renderer.h"
#include "lib/version.h"
#include "version.h"

static void print_usage(void) {
  printf("Usage:    namumark [file*]\n");
  printf("  --help, -h       Print usage information\n");
  printf("  --version        Print version\n");
  printf("  --html           Print HTML output (default)\n");
  printf("  --ast            Print AST JSON output\n");
}

int main(int argc, char *argv[]) {
  int i, file_counts = 0;
  int output_ast = 0;
  char **files;
  unsigned char buffer[4096];
  size_t bytes;
  namumark_parser *parser;
  namumark_node *document = NULL;
  int ret = 1;

  files = (char **)calloc(argc, sizeof(char *));

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("namumark lib.%s.bin.%s\n", NAMUMARK_LIB_VERSION, NAMUMARK_RUNTIME_VERSION);
      goto main_success;
    } else if (strcmp(argv[i], "--ast") == 0) {
      output_ast = 1;
    } else if (strcmp(argv[i], "--html") == 0) {
      output_ast = 0;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage();
      goto main_success;
    } else if (*argv[i] == '-') {
      print_usage();
      goto main_failure;
    } else {
      files[file_counts++] = argv[i];
    }
  }
  
  parser = parser_new();
  if (parser == NULL) {
    fprintf(stderr, "Failed to initialize parser\n");
    goto main_failure;
  }

  for (i = 0; i < file_counts; i++) {
    FILE *fp = fopen(files[i], "rb");
    if (fp == NULL) {
      fprintf(stderr, "Failed to open file: %s\n", files[i]);
      goto main_failure;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
      parser_feed(parser, buffer, bytes);
      if (bytes < sizeof(buffer)) { break; }
    }
    fclose(fp);
  }

  document = parser_finish(parser);
  if (!document ||
      (output_ast ? !print_document_ast(document, stdout)
                  : !print_document_html(document, stdout))) {
    goto main_failure;
  }

  namumark_node_free(document);
  document = NULL;
  parser_free(parser);
  free(files);
  files = NULL;
  parser = NULL;

main_success:
  ret = 0;
main_failure:

  if (document != NULL) {
    namumark_node_free(document);
  }
  if (parser != NULL) {
    parser_free(parser);
  }
  free(files);

  return ret;
}

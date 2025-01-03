#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/parser.h"
#include "lib/version.h"
#include "version.h"

void print_usage() {
  printf("Usage:    namumark [file*]\n");
  printf("  --help, -h       Print usage information\n");
  printf("  --version        Print version\n");
}

int main(int argc, char *argv[]) {
  int i, file_counts = 0;
  char **files;
  char buffer[4096];
  size_t bytes;
  namumark_parser *parser;
  int ret = 1;

  files = (char **)calloc(argc, sizeof(char *));

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("namumark lib.%s.bin.%s\n", NAMUMARK_LIB_VERSION, NAMUMARK_RUNTIME_VERSION);
      goto main_success;
    } else if (*argv[i] == '-') {
      print_usage();
      goto main_failure;
    } else {
      files[file_counts++] = argv[i];
    }
  }
  
  parser = parser_new();

  for (i = 0; i < file_counts; i++) {
    FILE *fp = fopen(files[i], "rb");
    if (fp == NULL) {
      fprintf(stderr, "Failed to open file: %s\n", files[i]);
      goto main_failure;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {

    }
    fclose(fp);
  }

main_success:
  ret = 0;
main_failure:

  return ret;
}

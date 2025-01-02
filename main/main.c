#include <stdio.h>
#include <string.h>

#include "lib/version.h"
#include "version.h"

void print_usage() {
  printf("Usage:    namumark [file*]\n");
  printf("  --help, -h       Print usage information\n");
  printf("  --version        Print version\n");
}

int main(int argc, char *argv[]) {
  int i, ret = 1;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("namumark lib.%s.bin.%s\n", NAMUMARK_LIB_VERSION, NAMUMARK_RUNTIME_VERSION);
      goto main_success;
    } else if (*argv[i] == '-') {
      print_usage();
      goto main_failure;
    }
  }
  
main_success:
  ret = 0;
main_failure:

  return ret;
}

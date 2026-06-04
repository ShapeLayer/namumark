# namumark

A parser that processes Namu Wiki's markup system. Syntax revision is based on the [(ko) Namuwiki Syntax Guide](https://namu.wiki/w/%EB%82%98%EB%AC%B4%EC%9C%84%ED%82%A4:%EB%AC%B8%EB%B2%95%20%EB%8F%84%EC%9B%80%EB%A7%90) accessed on 2026-06-03.  

## Build

```bash
cmake -S . -B build
cmake --build build
```

This builds both the CLI (`build/namumark`) and the shared C library (`build/libnamumark.*`).

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/namumark [file*]
```

## C API

Include `lib/namumark.h` and link against `libnamumark`.

```c
#include <stdio.h>
#include <string.h>
#include "lib/namumark.h"

int main(void) {
  const char *input = "== Title ==\nBody [[Page|link]]\n";
  namumark_buffer output = {0};
  namumark_status status = namumark_render_html(input, strlen(input), &output);
  if (status != NAMUMARK_OK) {
    fprintf(stderr, "%s\n", namumark_status_message(status));
    return 1;
  }
  fwrite(output.data, 1, output.size, stdout);
  namumark_buffer_free(&output);
  return 0;
}
```

The API also supports AST JSON output:

```c
namumark_render_ast_json(input, strlen(input), &output);
```

See `docs/namumark-api.3` for the manpage source. After install, view it with:

```bash
man 3 namumark-api
```

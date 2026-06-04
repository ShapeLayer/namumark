# namumark Python binding

This binding uses `ctypes` to call the shared C library built by CMake.

```bash
cmake -S ../.. -B ../../build
cmake --build ../../build
python3 - <<'PY'
from namumark import render_html
print(render_html('== 제목 ==\n본문 [[문서|링크]]\n'))
PY
```

Set `NAMUMARK_LIBRARY=/path/to/libnamumark.so` when the shared library is not in `../../build`.

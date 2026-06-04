# namumark Node.js binding

This binding uses `koffi` to call the shared C library built by CMake.

```bash
cmake -S ../.. -B ../../build
cmake --build ../../build
npm install
node - <<'JS'
const { renderHtml } = require('./namumark');
console.log(renderHtml('== 제목 ==\n본문 [[문서|링크]]\n'));
JS
```

Set `NAMUMARK_LIBRARY=/path/to/libnamumark.so` when the shared library is not in `../../build`.

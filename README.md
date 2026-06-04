# namumark

A parser that processes Namu Wiki's markup system. Syntax revision is based on the [(ko) Namuwiki Syntax Guide](https://namu.wiki/w/%EB%82%98%EB%AC%B4%EC%9C%84%ED%82%A4:%EB%AC%B8%EB%B2%95%20%EB%8F%84%EC%9B%80%EB%A7%90) accessed on 2026-06-03.  

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/namumark docs/namumark-base.txt
```


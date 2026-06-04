# namumark Rust binding

This crate links to the shared C library built by CMake.

```bash
cmake -S ../.. -B ../../build
cmake --build ../../build
RUSTFLAGS="-L ../../build" cargo test
```

At runtime, ensure `libnamumark` is discoverable by the platform dynamic linker, for example with `DYLD_LIBRARY_PATH=../../build` on macOS or `LD_LIBRARY_PATH=../../build` on Linux.

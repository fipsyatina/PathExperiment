# File preservation

Do not delete any file without first informing the user of its concrete path
and the reason. This includes temporary files, caches, downloaded archives,
generated artifacts, and cleanup performed by scripts or installers. Preserve
existing files. User instruction: «без мого відома не повинен бути видалений жоден файл».

# Project boundaries

- C++20. `pathx_core` and `include/pathexperiment/` must remain independent of SFML.
- Movement is four-way. Entry costs: road 1, plain 2, swamp 5. Walls are impassable.
- Route cost excludes the start; returned paths include both endpoints.
- One visual step expands at most one live cell and checks its neighbors.
- Benchmark search without rendering or artificial delays; show median timings.
- Map edits invalidate search and comparison results.
- Keep the five sample maps and real native captures in documentation.
- Never claim CI, native rendering, packaging or release checks without evidence.

# Validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The desktop application accepts `--smoke-test OUTPUT_DIR` and
`--record-demo OUTPUT_DIR`. Use a new output directory to preserve prior captures.
Headless validation uses `-DPATHEXPERIMENT_BUILD_APP=OFF`.

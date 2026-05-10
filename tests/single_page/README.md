# single_page smoke test

The first end-to-end test for fox-page. It also doubles as a starter
template you can copy when writing your own fox-page + fox-http app.

```
single_page/
├── CMakeLists.txt
├── README.md            (you are here)
├── run.sh               (drives the test: start, curl, assert, kill)
├── home.htpl            (the template — every directive in one file)
├── feature.h            (struct shared between template and main.cpp)
├── main.cpp             (tiny fox-http server with two routes + one static asset)
└── static/
    └── pico.min.css     (vendored Pico CSS v2.1.1, MIT)
```

## What it covers

- `{{expr}}` interpolation in text
- `cpp-text` directive (replaces inner text with an expression)
- `cpp-for` directive (renders the body once per element)
- `cpp-if` directive (renders the body when the condition holds)
- `<script type="application/x-cpp-head">` block injecting `#include`s
- `--func` with a multi-arg signature including STL types
- **SSO regression**: the feature list deliberately has 10 entries with
  short (≤15-byte) titles. The cpp-for body pushes ≥17 dynamic strings,
  enough to force any container that grows-by-reallocation (e.g. an old
  `std::vector` with `reserve(16)`) to move SSO strings to a new buffer
  and leave the matching iov pointers dangling. `run.sh` asserts every
  short title appears verbatim, catching this regression if it ever
  comes back.

## Prerequisites

- **fox-http** installed system-wide. From a fox-http checkout:
  ```bash
  cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
  cmake --build build -j && sudo cmake --install build
  ```
  After that, `find_package(fox-http REQUIRED)` resolves automatically.

The test does **not** need a separately-built fox-page binary — it uses
the in-tree target compiled by the same CMake invocation.

## Run as a test

From the repo root:

```bash
cmake -S . -B build -DFOX_PAGE_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure -R single_page_smoke
```

The test picks a random high port, starts the server in the background,
hits each route with `curl`, asserts status `200` and expected body
substrings, then tears down.

## Run manually (as a demo)

The `single_page_smoke` binary still serves the page indefinitely if you
run it directly:

```bash
./build/tests/single_page/single_page_smoke 8080 tests/single_page/static/pico.min.css
```

Then visit:

| URL | What you'll see |
|---|---|
| <http://0.0.0.0:8080/>            | `Hello, world!` + feature list |
| <http://0.0.0.0:8080/hello/fox>   | `Hello, fox!` |
| <http://0.0.0.0:8080/hello/yelin> | `Hello, yelin!` |

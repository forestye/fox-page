# fox-page example — single-page demo

Minimal end-to-end demo that exercises every fox-page directive
(`{{expr}}`, `cpp-text`, `cpp-if`, `cpp-for`) and serves the result over
fox-http with a single `writev`.

```
example/
├── home.htpl              # template, rendered into home.cpp at build time
├── feature.h              # shared struct visible to template + main.cpp
├── main.cpp               # tiny fox-http server with two routes
├── static/
│   └── pico.min.css       # vendored Pico CSS v2.1.1, MIT
└── CMakeLists.txt         # standalone — not wired into the top-level build
```

## Prerequisites

- **fox-page** built at `../build/fox-page` (run `cmake -S .. -B ../build &&
  cmake --build ../build` from this directory once), or available on PATH,
  or pass `-DFOX_PAGE=/path/to/fox-page`.
- **fox-http** source at `../../fox-http` (a sibling clone), or pass
  `-DFOX_HTTP_DIR=/path/to/fox-http`.

## Build & run

```bash
cd example
cmake -S . -B build
cmake --build build -j
./build/fox-page-demo            # listens on 8080
```

Then visit:

| URL | What you'll see |
|---|---|
| <http://127.0.0.1:8080/>            | `Hello, world!` + feature list |
| <http://127.0.0.1:8080/hello/fox>   | Same page with `Hello, fox!` |
| <http://127.0.0.1:8080/hello/yelin> | Same page with `Hello, yelin!` |

Pass a different port as the first argument:

```bash
./build/fox-page-demo 9090
```

## How it works

At build time, fox-page reads `home.htpl` and generates a regular C++
function in `build/home.cpp`:

```cpp
void render_home(fox::http::HttpResponse& resp,
                 const std::string& name,
                 const std::string& timestamp,
                 const std::vector<Feature>& features);
```

The function body is a sequence of `static constexpr std::string_view`
fragments (the literal HTML) interleaved with `stringify(...)` of the
dynamic expressions, all packed into one `iovec` array and flushed with
`resp.writev(...)`. Per request, no template parsing, no allocation for
static fragments.

`main.cpp` calls that function from two routes (`/` and `/hello/:name`)
and serves the vendored CSS from `/static/pico.min.css`.

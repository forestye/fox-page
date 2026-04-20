# fox-page

HTML 模板 → C++ 代码生成器，为 [fox-http](https://github.com/forestye/fox-http)
生成零拷贝的 `HttpResponse::writev` 响应函数。

## 做什么

读取带 `cpp-*` 指令或 `{{expr}}` 插值的 HTML/htpl 文件，输出一个 C++ 函数，
函数体把静态 HTML 片段（编译期 `string_view` 字面量）+ 动态表达式结果打包
成 `iovec` 数组，一次 `writev` 发到 socket。

**核心收益：**
- **零拷贝**：静态 HTML 直接引用 `.rodata` 段，不复制到堆/栈。
- **单次 syscall**：所有片段一次 `writev` 发出；和 fox-http 的 Immediate 模式
  配合后会把 status-line + headers 也合并进同一 `iovec`，整个响应 1 次
  syscall。
- **编译期验证**：生成的是普通 C++，模板里的表达式会被编译器检查类型、语法，
  错了在 build 期暴露，不是在请求时才崩。

---

## 依赖

- C++17 编译器（GCC 9+ / Clang 10+）
- CMake 3.16+
- [Lexbor](https://github.com/lexbor/lexbor) —— HTML 解析库。未安装会自动
  通过 `FetchContent` 拉 v2.3.0 源码构建
- 运行时无依赖；生成的代码只依赖 fox-http

---

## 构建

```bash
git clone git@github.com:forestye/fox-page.git
cd fox-page
cmake -S . -B build
cmake --build build -j
# 可执行文件：./build/fox-page
# 可选安装：sudo cmake --install build
```

---

## 模板语法

### 1. `{{expression}}` 插值

在文本节点或属性里嵌入 C++ 表达式，值会通过 `stringify(...)` 变成字符串输出。

```html
<p>Hello {{name}}! You have {{count}} messages.</p>
<img cpp:src="user.avatar" />
<a cpp:href="'/user/' + std::to_string(id)">profile</a>
```

→ 对应生成的 iov 片段：静态 `<p>Hello ` + 动态 `name` + 静态 `! You have ` +
动态 `count` + 静态 ` messages.</p>`，按序摆进 `iov_list`。

### 2. `cpp-*` 指令

| 指令 | 作用 |
|---|---|
| `cpp-text="expr"` | 节点内部文本替换为表达式结果，子节点忽略 |
| `cpp-if="cond"` | 条件渲染（`if (cond) { ... }` 包裹节点输出） |
| `cpp-for="loop"` | 循环渲染（`for (loop) { ... }` 包裹节点输出） |
| `cpp:<attr>="expr"` | 动态属性（如 `cpp:href`、`cpp:src`、`cpp:class`） |

```html
<ul cpp-if="!users.empty()">
    <li cpp-for="const auto& u : users" cpp-text="u.name"></li>
</ul>
```

### 3. 代码注入块

放在 `<script>` 标签里，生成器按 `type` 分类搬到对应位置：

```html
<script type="application/x-cpp-head">
    // 放到生成文件的文件头（include、using 等）
    #include <ctime>
    #include <sstream>
</script>

<script type="application/x-cpp-source">
    // 放到生成函数的函数体顶部（局部变量、预计算）
    std::time_t t = std::time(nullptr);
    auto* tm = std::localtime(&t);
    std::string today = std::to_string(tm->tm_year + 1900) + "-"
                      + std::to_string(tm->tm_mon + 1);
</script>

<p>Today is {{today}}</p>
```

### 4. 旧版语法兼容

- **`<% C++ 语句 %>`**：原样插入 C++ 代码，不包 `out <<`
- 插值语法 `{{expr}}` 也在旧版里生效

---

## 用法

### 基础

```bash
fox-page template.html                          # → ./template.cpp
fox-page template.html -o myfile.cpp            # 指定输出文件名
fox-page *.html -o generated/                   # 批量生成到目录
```

### 自定义函数签名

生成函数默认签名是 `void <stem>(HttpResponse& resp)`。用 `--func` 覆盖：

```bash
fox-page user.html --func "void render_user(HttpResponse& resp, const User& u, int id)"
```

特殊 token `__func__` 会被替换成**文件名**（无扩展）：

```bash
fox-page user.html --func "void __func__(HttpResponse& resp, int id)"
#   等价于   --func "void user(HttpResponse& resp, int id)"
```

这个模式方便和 [`fox-route`](https://github.com/forestye/fox-route) 的
`fox-route-func` 工具配合——它从 `.crdl` 文件里抽出某个 handler 的签名，
喂给 fox-page 作为 `--func` 参数，保证路由和模板函数的参数列表完全一致。

### 注入外部代码块

如果多个模板需要共享一段头部代码或 helper：

```bash
fox-page common.html --extra common_block.html -o generated/
```

`--extra` 指向的文件里可以写 `x-cpp-head` / `x-cpp-source` / `x-cpp-tail`
三种 script 块，内容会合并到生成代码的相应位置。

---

## 生成代码示例

输入 `hello.html`：

```html
<p>Hello {{name}}! Today is {{today}}.</p>
```

调用：

```bash
fox-page hello.html --func "void hello(HttpResponse& resp, std::string name)"
```

输出 `hello.cpp`（简化显示）：

```cpp
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <sys/uio.h>
#include "fox-http/http_response.h"
#include "fox-http/http_util.h"

using fox::http::HttpResponse;
using fox::http::util::stringify;

void hello(HttpResponse& resp, std::string name) {
    resp.set_status(200);
    resp.headers().content_type("text/html; charset=utf-8");

    std::vector<std::string> dynamic_strings;
    std::vector<iovec> iov_list;

    static constexpr std::string_view body0 = "<p>Hello ";
    iov_list.push_back({(void*)body0.data(), body0.size()});
    dynamic_strings.push_back(stringify(name));
    iov_list.push_back({(void*)dynamic_strings.back().data(),
                        dynamic_strings.back().size()});
    static constexpr std::string_view body1 = "! Today is ";
    iov_list.push_back({(void*)body1.data(), body1.size()});
    dynamic_strings.push_back(stringify(today));
    iov_list.push_back({(void*)dynamic_strings.back().data(),
                        dynamic_strings.back().size()});
    static constexpr std::string_view body2 = ".</p>";
    iov_list.push_back({(void*)body2.data(), body2.size()});

    size_t total_size = 0;
    for (const auto& iov : iov_list) total_size += iov.iov_len;
    resp.headers().content_length(total_size);

    if (!resp.writev(iov_list.data(), iov_list.size())) {
        std::cerr << "writev failed, expected=" << total_size << std::endl;
    }
}
```

关键设计点：
- 静态片段用 `static constexpr std::string_view` —— 零拷贝，指向 `.rodata`。
- 动态值先保存在 `dynamic_strings`（栈上的 `vector<string>`），再把数据指针
  塞进 `iov_list`；栈对象的生命周期覆盖整个 `writev` 调用，零拷贝成立。
- 一次 `resp.writev(...)` 完成。fox-http 的 Immediate 模式会把 `HTTP/1.1 ...`
  状态行 + headers 预置到 iov[0]，整个响应只有一次 syscall。

---

## 和 fox-http 的集成

在你的 CMakeLists.txt 里，把 `.html` 模板声明为 build 期生成源：

```cmake
find_program(FOX_PAGE_EXECUTABLE NAMES fox-page
    HINTS ${CMAKE_SOURCE_DIR}/../fox-page/build)

set(PAGES index user profile)
set(GENERATED_PAGE_CPP)
foreach(page ${PAGES})
    set(HTML ${CMAKE_SOURCE_DIR}/pages/${page}.html)
    set(CPP  ${CMAKE_BINARY_DIR}/pages/${page}.cpp)
    add_custom_command(
        OUTPUT ${CPP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/pages
        COMMAND ${FOX_PAGE_EXECUTABLE} ${HTML} -o ${CMAKE_BINARY_DIR}/pages
                --func "void ${page}(fox::http::HttpResponse& resp)"
        DEPENDS ${HTML}
    )
    list(APPEND GENERATED_PAGE_CPP ${CPP})
endforeach()

add_executable(my_app main.cpp ${GENERATED_PAGE_CPP})
target_link_libraries(my_app PRIVATE fox-http::fox-http)
```

调用示例（配合 fox-http 的 Router）：

```cpp
#include "fox-http/http_router.h"
void index(fox::http::HttpResponse& resp);  // from generated/index.cpp

fox::http::HttpRouter router;
router.get("/", [](auto&, auto& resp) { index(resp); });
```

---

## 姊妹项目

- [fox-http](https://github.com/forestye/fox-http) —— 运行时 HTTP 服务器库
- [fox-route](https://github.com/forestye/fox-route) —— CRDL → C++ Router
  代码生成器。其 `fox-route-func` 工具可抽出路由签名喂给 fox-page 的 `--func`
  参数，保证路由和模板参数一致

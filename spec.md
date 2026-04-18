# **项目需求描述：Weave++ 模板转译器**

## **1. 项目名称**

Weave++

---

## **2. 项目概述与目标**

Weave++ 是一个基于 C++17 开发的命令行工具。其核心功能是将带有特定 `cpp-*` 标记的 HTML 模板文件（例如 `.htpl` 文件）转译成高性能、自包含的 C++ 源代码文件（`.cpp`）。

该项目旨在让开发者能够使用标准的 HTML 语法来设计页面结构，同时将动态逻辑通过内嵌 C++ 表达式的方式注入模板。  
转译后生成的 C++ 代码可以直接编译进最终应用程序，用于在服务器端或本地高效生成动态 HTML 内容，从而避免运行时解析模板的开销。

此外，项目已扩展支持 **旧版模板语法**，以增强灵活性并兼容历史模板。  

---

## **3. 核心技术栈**

- **编程语言：** C++17  
- **HTML 解析库：** Lexbor  
  - 使用现代 C++ 的 RAII 封装 C API，确保内存安全  
- **构建系统：** CMake（通过 `FetchContent` 集成 Lexbor）  
- **依赖性：** 除 C++ 标准库和 Lexbor 外无其他依赖  

---

## **4. 功能需求**

### **4.1. 输入与输出**

- **输入：** 任意后缀的模板文件（如 `.htpl`、`.html`、`.tpl` 等）
- **输出：** 每个模板生成对应的 `.cpp` 源代码文件，包含渲染函数  

### **4.2. 模板语法与转译规则**

#### **A. 静态 HTML 内容**

- 普通 HTML 标签、文本、注释 → 作为字符串输出到流 `out << "..."`  
- 必须正确转义 `"`、`\` 等字符  

#### **B. 动态属性与指令 (`cpp-*`)**

1. **`cpp-text="<expression>"`**  
   - 将内部文本替换为表达式结果  
   - 子节点被忽略  
   - 示例: `<span cpp-text="user.getName()">` → `out << "<span>" << (user.getName()) << "</span>";`  

2. **`cpp:attribute="<expression>"` (动态属性)**  
   - 以 `cpp:` 开头的属性 → 去前缀后作为表达式结果填充  
   - 示例: `<a cpp:href="item.url">` → `out << "<a href=\"" << (item.url) << "\">";`  

3. **`cpp-if="<condition>"`**  
   - 条件渲染 → 用 `if` 包裹代码块  

4. **`cpp-for="<loop_expression>"`**  
   - 循环渲染 → 用 C++ 范围 `for` 包裹代码块  

#### **C. 旧版语法支持**

1. **代码执行块 `<% ... %>`**
   - 提取其中的 C++ 代码，直接写入输出，不包裹 `out <<`
   - 示例:
     ```html
     <div><% int i = 10; %></div>
     ```
     转译为：
     ```cpp
     out << "<div>";
     int i = 10;
     out << "</div>";
     ```

2. **表达式插值 `{{...}}`**
   - 可出现在文本节点或属性值中
   - **文本节点：**
     将字符串拆分为静态与动态部分 → 静态 `out << "..."`，动态 `out << (expr)`
   - **属性值：**
     同样拆分处理，但不适用于 `cpp-*` 和 `cpp:` 属性
   - 示例:
     ```html
     <p>User: {{user.getName()}}, Age: {{user.getAge()}}</p>
     ```
     转译为：
     ```cpp
     out << "<p>User: " << (user.getName()) << ", Age: " << (user.getAge()) << "</p>";
     ```

#### **D. Script 标签支持**

支持三种特殊的 `<script>` 标签用于 C++ 代码块：

1. **`<script type="application/x-cpp-source">`** (源码内联块)
   - 等效于 `<% ... %>`，内部代码插入到生成函数体的对应位置
   - 用于声明局部变量、执行逻辑语句等
   - 示例:
     ```html
     <div>
         <script type="application/x-cpp-source">
             auto age = context.user.getAge();
         </script>
         <p>Age: {{age}}</p>
     </div>
     ```
     转译为：
     ```cpp
     out << "<div>";
     auto age = context.user.getAge();
     out << "<p>Age: " << (age) << "</p>";
     out << "</div>";
     ```

2. **`<script type="application/x-cpp-head">`** (头部预置块)
   - 内部代码提取到生成的 `.cpp` 文件顶部
   - 用于声明头文件、定义辅助函数、类型别名等
   - 示例:
     ```html
     <script type="application/x-cpp-head">
         #include <cmath>
         double square(double x) { return x * x; }
     </script>
     <div>Value: {{square(5)}}</div>
     ```
     生成的代码文件：
     ```cpp
     #include <iostream>
     #include <string>
     #include <string_view>
     #include <sys/uio.h>
     #include <photon/net/http/server.h>
     #include <photon/common/alog-stdstring.h>
     #include <type_traits>

     using namespace photon;
     using namespace photon::net;
     using namespace photon::net::http;

     #include <cmath>
     double square(double x) { return x * x; }

     // Helper function to convert various types to string
     template<typename T>
     inline std::string stringify(T&& val) {
         using DecayT = typename std::decay<T>::type;
         if constexpr (std::is_same_v<DecayT, std::string> ||
                       std::is_same_v<DecayT, const char*> ||
                       std::is_convertible_v<DecayT, std::string_view>) {
             return std::string(std::forward<T>(val));
         } else {
             return std::to_string(std::forward<T>(val));
         }
     }

     void render_template(Response& resp) {
         resp.set_result(200);
         resp.headers.insert("Content-Type", "text/html; charset=utf-8");

         // ... 向量化I/O输出代码 ...
     }
     ```

3. **`<script type="application/x-cpp-tail">`** (尾部预置块)
   - 内部代码插入到生成函数的末尾（在向量化I/O输出之后）
   - 用于资源清理、日志记录、统计等收尾工作
   - 示例:
     ```html
     <div>{{message}}</div>
     <script type="application/x-cpp-tail">
         cleanup_resources();
         log_request_completed();
     </script>
     ```
     生成的函数：
     ```cpp
     void render_template(Response& resp) {
         resp.set_result(200);
         resp.headers.insert("Content-Type", "text/html; charset=utf-8");

         // ... 向量化I/O输出代码 ...
         ssize_t ret = resp.writev(iov, count);
         if (ret != static_cast<ssize_t>(body_size)) {
             LOG_ERROR(0, "send body failed, ret=", ret, ", expected=", body_size);
         }
         cleanup_resources();
         log_request_completed();
     }
     ```

---

## **5. 生成代码结构**

Weave++ 使用高性能的向量化I/O输出模式：

### **向量化I/O模式（唯一输出模式）**

#### **生成代码的整体结构**
每个 `.cpp` 文件按以下顺序组织：

1. **标准头文件包含**
   ```cpp
   #include <iostream>
   #include <string>
   #include <string_view>
   #include <sys/uio.h>  // for iovec
   #include <type_traits>
   ```

2. **Photon 库头文件**
   ```cpp
   #include <photon/net/http/server.h>
   #include <photon/common/alog-stdstring.h>
   ```

3. **命名空间声明**
   ```cpp
   using namespace photon;
   using namespace photon::net;
   using namespace photon::net::http;
   ```

4. **用户自定义头部代码**（来自 `x-cpp-head` 标签）

5. **stringify() 智能类型转换函数**

6. **渲染函数**
   - 函数签名：`void <function_name>(Response& resp);`
   - 函数名规则：由文件名生成（不含目录前缀）
     - 例如：`user_profile.html` → `user_profile`
     - 例如：`admin/dashboard.html` → `dashboard`

#### **生成特性**
- 静态字符串使用 `std::string_view` 常量（零拷贝）
- 动态内容通过 `stringify()` 智能转换为 `std::string`
- 函数开始处自动设置 HTTP 状态码 200 和 Content-Type
- 自动计算总大小并设置 Content-Length
- 使用 `iovec` 结构进行向量化写入
- 单次 `writev()` 系统调用减少I/O开销

#### **代码生成示例**
输入模板（`template.html`）：
```html
<p>Hello {{name}}! Your ID is {{id}}.</p>
```

生成代码：
```cpp
#include <iostream>
#include <string>
#include <string_view>
#include <sys/uio.h>  // for iovec
#include <photon/net/http/server.h>
#include <photon/common/alog-stdstring.h>
#include <type_traits>

using namespace photon;
using namespace photon::net;
using namespace photon::net::http;


// Helper function to convert various types to string
template<typename T>
inline std::string stringify(T&& val) {
    using DecayT = typename std::decay<T>::type;
    if constexpr (std::is_same_v<DecayT, std::string> ||
                  std::is_same_v<DecayT, const char*> ||
                  std::is_convertible_v<DecayT, std::string_view>) {
        return std::string(std::forward<T>(val));
    } else {
        return std::to_string(std::forward<T>(val));
    }
}

void template(Response& resp) {
    resp.set_result(200);
    resp.headers.insert("Content-Type", "text/html; charset=utf-8");

    static constexpr std::string_view body0 = "<p>Hello ";
    std::string body1 = stringify(name);
    static constexpr std::string_view body2 = "! Your ID is ";
    std::string body3 = stringify(id);
    static constexpr std::string_view body4 = ".</p>\n";
    size_t body_size = body0.size();
    body_size += body1.size();
    body_size += body2.size();
    body_size += body3.size();
    body_size += body4.size();
    resp.headers.content_length(body_size);

    // 向量化写入避免多次系统调用
    struct iovec iov[5];
    iov[0].iov_base = (void*)body0.data();
    iov[0].iov_len = body0.size();
    iov[1].iov_base = (void*)body1.data();
    iov[1].iov_len = body1.size();
    iov[2].iov_base = (void*)body2.data();
    iov[2].iov_len = body2.size();
    iov[3].iov_base = (void*)body3.data();
    iov[3].iov_len = body3.size();
    iov[4].iov_base = (void*)body4.data();
    iov[4].iov_len = body4.size();
    ssize_t ret = resp.writev(iov, 5);
    if (ret != static_cast<ssize_t>(body_size)) {
        LOG_ERROR(0, "send body failed, ret=", ret, ", expected=", body_size);
    }
}
```

### **stringify() 智能类型转换函数**

生成的代码包含一个模板函数 `stringify()`，用于智能处理不同类型的值转换为字符串：

```cpp
template<typename T>
inline std::string stringify(T&& val) {
    using DecayT = typename std::decay<T>::type;
    if constexpr (std::is_same_v<DecayT, std::string> ||
                  std::is_same_v<DecayT, const char*> ||
                  std::is_convertible_v<DecayT, std::string_view>) {
        return std::string(std::forward<T>(val));
    } else {
        return std::to_string(std::forward<T>(val));
    }
}
```

**功能特点：**
- 对于 `std::string`、`const char*` 和 `std::string_view` 类型，直接转换为 `std::string`
- 对于其他类型（整数、浮点数等），调用 `std::to_string()` 进行转换
- 避免了对字符串类型错误调用 `to_string()` 导致的编译错误
- 使用完美转发和编译期 `if constexpr` 实现零开销抽象

### **HTTP 响应初始化**

生成的函数在开始处自动添加 HTTP 响应初始化代码：

```cpp
void template(Response& resp) {
    resp.set_result(200);
    resp.headers.insert("Content-Type", "text/html; charset=utf-8");

    // ... 后续渲染代码 ...
}
```

**自动设置：**
- HTTP 状态码：200 (OK)
- Content-Type：`text/html; charset=utf-8`

---

## **6. 错误处理**

- 输入文件无效或不可读 → 报错并退出  
- 输出目录不可写 → 报错并退出  
- 模板语法错误处理：  
  - **未闭合插值** `{{` 或 `}}` → 报错并指明位置  
  - **嵌套插值**（如 `{{user.get("{{field}}")}}`） → 报错  

---

## **7. 非功能性需求**

- **性能：** 转译高效，生成代码无额外开销  
- **代码风格：** 输出 C++ 代码清晰、缩进规范、可读性好  

---

## **8. 命令行接口 (CLI)**

### **基本用法**
- `weave++ <input_file> [more_files...] -o <output_directory>`
- `weave++ --func <signature> <input_file> [more_files...]`
- `weave++ --help`
- `weave++ --version`

### **选项**
- `-o <path>` - 指定输出文件或目录（默认：当前目录）
  - **判断规则**：
    1. 如果路径已存在，以现状为准（文件则为文件，目录则为目录）
    2. 如果以 `/` 结尾，则为目录
    3. 如果以 `.cpp` 结尾，则为文件
    4. 其余情况均认为是文件
  - **多文件输入时**：`-o` 必须指定为目录
- `--func <signature>` - 自定义函数签名（默认：`"void __func__(Response& resp)"`）
- `--extra <file>` - 注入额外的 C++ 代码文件

### **函数签名自定义**

`--func` 参数支持三种用法：

1. **完全指定函数签名**
   ```bash
   weave++ --func "void render_page(HttpResponse& resp, int user_id)" template.html
   ```

2. **使用 `__func__` 占位符自动生成函数名**
   ```bash
   weave++ --func "int __func__(Output& out, const Context& ctx)" *.html
   ```
   - 文件 `user_profile.html` → 函数名 `user_profile`
   - 文件 `admin/dashboard.html` → 函数名 `dashboard`（只使用文件名，不含目录前缀）

3. **使用默认签名**（省略 `--func` 参数）
   ```bash
   weave++ template.html  # 等同于 --func "void __func__(Response& resp)"
   ```

### **输出路径控制**

`-o` 参数支持灵活的文件和目录输出：

```bash
# 输出到当前目录（默认）
weave++ template.html                    # 生成 ./template.cpp

# 输出到指定文件名
weave++ template.html -o myfile.cpp      # 生成 myfile.cpp
weave++ template.html -o result          # 生成 result（无扩展名文件）

# 输出到目录（目录必须以 / 结尾或已存在）
weave++ template.html -o output/         # 生成 output/template.cpp
weave++ template.html -o build/          # 生成 build/template.cpp（会自动创建目录）

# 多文件必须输出到目录
weave++ *.html -o generated/             # 正确：输出到目录
weave++ file1.html file2.html -o out.cpp # 错误：多文件不能指定单个输出文件
```

### **一般用法示例**
```bash
# 批量处理不同后缀的文件
weave++ *.html *.tpl *.htpl -o headers/

# 混合不同类型的模板文件
weave++ user_profile.htpl product_list.html footer.tpl -o output/

# 自定义函数签名示例
weave++ --func "void render(WebResponse& res)" template.html -o myrender.cpp
weave++ --func "std::string __func__(const Data& data)" *.html -o generated/

# 代码注入示例
weave++ --extra common.html template.html -o output/
weave++ --extra utils.html --func "void __func__(HttpResponse& resp)" *.html -o build/
```

### **代码注入功能**

`--extra` 参数允许从外部文件注入 C++ 代码，支持模块化和代码复用：

#### **注入文件格式**
注入文件应为包含特殊 `<script>` 标签的 HTML 文件：

```html
<html>
<body>
<script type="application/x-cpp-head">
#include <vector>
#include <memory>
using Numbers = std::vector<int>;
std::shared_ptr<Logger> logger = std::make_shared<Logger>();
</script>

<script type="application/x-cpp-source">
logger->info("Starting template rendering");
auto start_time = std::chrono::high_resolution_clock::now();
</script>

<script type="application/x-cpp-tail">
auto end_time = std::chrono::high_resolution_clock::now();
logger->info("Rendering took {} microseconds",
    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count());
</script>
</body>
</html>
```

#### **注入顺序**
生成的代码按以下顺序组织：

1. **标准头文件** - `#include <iostream>` 等
2. **外部头部代码** - 注入文件的 `x-cpp-head` 内容
3. **模板头部代码** - 模板内部的 `x-cpp-head` 内容
4. **函数开始**
5. **外部源码** - 注入文件的 `x-cpp-source` 内容
6. **模板内容** - 模板的 HTML 渲染和内部 `x-cpp-source`
7. **外部尾部代码** - 注入文件的 `x-cpp-tail` 内容
8. **模板尾部代码** - 模板内部的 `x-cpp-tail` 内容
9. **函数结束**

#### **使用场景**
- **通用工具函数** - 在多个模板间共享辅助函数
- **日志和监控** - 统一的性能监控和日志记录
- **资源管理** - 统一的资源初始化和清理
- **类型定义** - 共享的类型别名和常量定义

```

---

## **9. 总结**

Weave++ 现已同时支持：
- 结构化的 **`cpp-*` 指令系统**
- 灵活的 **旧版 `<% ... %>` 和 `{{...}}` 语法**
- 强大的 **`<script>` 标签支持**，用于内联代码和头部声明
- **高性能向量化I/O输出**，作为唯一输出模式，显著减少系统调用开销

这种全面的功能集合使模板既能保持可维护性，又能在复杂场景中拥有更强表达力和更优性能。Weave++ 专注于高性能服务器端应用，通过向量化I/O提供最优的性能表现。  

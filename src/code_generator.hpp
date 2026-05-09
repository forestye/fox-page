#pragma once

#include <string>
#include "lexbor_wrapper.hpp"
#include "template_processor.hpp"

namespace weave {

struct FunctionSignature {
    std::string return_type;
    std::string function_name;
    std::string parameters;
};

struct InjectionContent {
    std::string head_code;    // x-cpp-head content
    std::string source_code;  // x-cpp-source content
    std::string tail_code;    // x-cpp-tail content
};

class CodeGenerator {
public:
    // 生成 .cpp 文件：向量化 I/O 函数体 + 自定义签名 + 可选 --extra 注入
    std::string generate_header_with_injection(const std::string& html_content, const std::string& function_signature, const std::string& input_file_path, const std::string& injection_file_path);

private:
    std::string generate_function_body(const std::string& html_content);
    std::string generate_vectorized_code(const std::vector<OutputSegment>& segments);

    // 工具方法
    std::string escape_string(const std::string& str);
    std::vector<OutputSegment> merge_consecutive_static_strings(const std::vector<OutputSegment>& segments);

    // 函数签名解析和处理方法
    FunctionSignature parse_function_signature(const std::string& signature);
    std::string generate_function_name_from_path(const std::string& file_path);
    std::string sanitize_identifier(const std::string& input);

    // 注入文件解析和处理方法
    InjectionContent parse_injection_file(const std::string& file_path);
    std::string wrap_in_header_with_injection(const std::string& body_code, const std::string& template_head_code, const std::string& template_tail_code, const std::string& function_declaration, const InjectionContent& injection);

    TemplateProcessor processor_;
};

} // namespace weave
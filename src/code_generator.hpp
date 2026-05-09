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
    // 主要生成方法 - 现在仅使用向量化I/O
    std::string generate_header(const std::string& html_content, const std::string& function_name = "render_template");

    // 新方法：使用自定义函数签名生成代码
    std::string generate_header_with_signature(const std::string& html_content, const std::string& function_signature, const std::string& input_file_path);

    // 新方法：支持注入文件的代码生成
    std::string generate_header_with_injection(const std::string& html_content, const std::string& function_signature, const std::string& input_file_path, const std::string& injection_file_path);

private:
    // 向量化I/O生成方法（现在是唯一方法）
    std::string generate_function_body(const std::string& html_content);
    std::string wrap_in_header(const std::string& body_code, const std::string& head_code, const std::string& function_name);
    std::string wrap_in_header_with_custom_signature(const std::string& body_code, const std::string& head_code, const std::string& function_declaration);
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
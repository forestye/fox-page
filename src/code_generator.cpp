#include "code_generator.hpp"
#include <sstream>
#include <fstream>
#include <iostream>

namespace weave {

std::string CodeGenerator::generate_header(const std::string& html_content, const std::string& function_name) {
    try {
        processor_.reset(); // Clear any previous state
        std::string body_code = generate_function_body(html_content);
        std::string head_code = processor_.get_head_code();
        return wrap_in_header(body_code, head_code, function_name);
    } catch (const std::exception& e) {
        return "// Error: " + std::string(e.what()) + "\n";
    }
}

std::string CodeGenerator::generate_function_body(const std::string& html_content) {
    try {
        // In vectorized mode, we don't need preprocessing
        // process_script_element_vectorized handles script tags directly
        LexborDocument doc(html_content);

        std::vector<OutputSegment> segments;

        // First, check for DOCTYPE declaration
        lxb_html_document_t* html_doc = doc.get();
        if (html_doc) {
            lxb_dom_document_t* dom_doc = lxb_dom_interface_document(html_doc);
            if (dom_doc && dom_doc->doctype) {
                // Add DOCTYPE as a static string
                segments.push_back(OutputSegment(OutputSegment::STATIC_STRING, "<!DOCTYPE html>\n"));
            }
        }

        // Now process the HTML element (includes <html>, <head>, <body>, etc.)
        lxb_dom_node_t* root = doc.document_element();  // Get <html> element
        if (!root) {
            return "// Empty document\n";
        }

        auto root_segments = processor_.process_node_vectorized(root);
        segments.insert(segments.end(), root_segments.begin(), root_segments.end());

        return generate_vectorized_code(segments);
    } catch (const std::exception& e) {
        return "// Error: " + std::string(e.what()) + "\n";
    }
}

std::string CodeGenerator::generate_vectorized_code(const std::vector<OutputSegment>& segments) {
    std::ostringstream result;

    // Set HTTP response status and headers
    result << "    resp.set_status(200);\n";
    result << "    resp.headers().content_type(\"text/html; charset=utf-8\");\n";
    result << "\n";

    // 合并连续的静态字符串以优化输出
    std::vector<OutputSegment> merged_segments = merge_consecutive_static_strings(segments);

    // Initialize containers
    result << "    // 动态字符串容器，保持生命周期到函数结束\n";
    result << "    std::vector<std::string> dynamic_strings;\n";
    result << "    dynamic_strings.reserve(16);\n";
    result << "\n";
    result << "    // 动态构建 iovec 数组\n";
    result << "    std::vector<iovec> iov_list;\n";
    result << "    iov_list.reserve(32);\n";
    result << "\n";

    int variable_counter = 0;

    // Process all segments in original order
    for (const auto& segment : merged_segments) {
        if (segment.type == OutputSegment::CODE_BLOCK) {
            // 直接输出代码块
            result << "    " << segment.content << "\n";
        } else if (segment.type == OutputSegment::STATIC_STRING) {
            // 静态字符串：声明变量并添加到 iov_list
            std::string var_name = "body" + std::to_string(variable_counter++);
            result << "    static constexpr std::string_view " << var_name
                   << " = \"" << escape_string(segment.content) << "\";\n";
            result << "    iov_list.push_back({(void*)" << var_name
                   << ".data(), " << var_name << ".size()});\n";
        } else if (segment.type == OutputSegment::DYNAMIC_EXPR) {
            // 动态表达式：先保存到容器，然后从容器获取指针
            result << "    dynamic_strings.push_back(stringify(" << segment.content << "));\n";
            result << "    iov_list.push_back({(void*)dynamic_strings.back().data(), dynamic_strings.back().size()});\n";
        }
    }

    // Calculate total size
    result << "\n";
    result << "    // 计算总大小\n";
    result << "    size_t total_size = 0;\n";
    result << "    for (const auto& iov : iov_list) {\n";
    result << "        total_size += iov.iov_len;\n";
    result << "    }\n";
    result << "    resp.headers().content_length(total_size);\n";
    result << "\n";

    // Generate writev call. Zero-copy: iov points to static literals and
    // dynamic_strings owned on the stack until the handler returns.
    result << "    // Vectorized write — single syscall, zero-copy for static fragments.\n";
    result << "    if (!resp.writev(iov_list.data(), iov_list.size())) {\n";
    result << "        std::cerr << \"writev failed for response body, expected=\" << total_size << std::endl;\n";
    result << "    }\n";

    return result.str();
}

std::string CodeGenerator::wrap_in_header(const std::string& body_code, const std::string& head_code, const std::string& function_name) {
    std::ostringstream result;

    // Includes
    result << "#include <iostream>\n";
    result << "#include <string>\n";
    result << "#include <string_view>\n";
    result << "#include <vector>\n";
    result << "#include <sys/uio.h>  // for iovec\n";
    result << "#include \"httpserver/http_response.h\"\n";
    result << "#include \"httpserver/http_util.h\"\n";
    result << "\n";
    result << "using httpserver::HttpResponse;\n";
    result << "using httpserver::util::stringify;\n";
    result << "\n";

    // Add head code if any
    if (!head_code.empty()) {
        result << head_code << "\n";
    }

    result << "\n";

    // Function signature. Default weave++ signature now takes HttpResponse&.
    result << "void " << function_name << "(httpserver::HttpResponse& resp) {\n";

    // Function body
    std::istringstream body_stream(body_code);
    std::string line;
    while (std::getline(body_stream, line)) {
        result << line << "\n";
    }

    result << "}\n";

    return result.str();
}

// Helper method for escaping strings (reuse existing logic)
std::string CodeGenerator::escape_string(const std::string& str) {
    std::ostringstream result;
    for (char c : str) {
        switch (c) {
            case '"':  result << "\\\""; break;
            case '\\': result << "\\\\"; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            default:   result << c; break;
        }
    }
    return result.str();
}

// 合并连续的静态字符串段以优化输出
std::vector<OutputSegment> CodeGenerator::merge_consecutive_static_strings(const std::vector<OutputSegment>& segments) {
    std::vector<OutputSegment> merged;

    if (segments.empty()) {
        return merged;
    }

    size_t i = 0;
    while (i < segments.size()) {
        const auto& current = segments[i];

        if (current.type == OutputSegment::STATIC_STRING) {
            // 找到一个静态字符串，看看后面还有多少个连续的静态字符串
            std::string combined_content = current.content;
            size_t j = i + 1;

            // 合并所有连续的静态字符串
            while (j < segments.size() && segments[j].type == OutputSegment::STATIC_STRING) {
                combined_content += segments[j].content;
                j++;
            }

            // 创建合并后的段
            OutputSegment merged_segment(OutputSegment::STATIC_STRING, combined_content);
            merged.push_back(merged_segment);

            // 移动到下一个非静态字符串段
            i = j;
        } else {
            // 非静态字符串段，直接添加
            merged.push_back(current);
            i++;
        }
    }

    return merged;
}

std::string CodeGenerator::generate_header_with_signature(const std::string& html_content, const std::string& function_signature, const std::string& input_file_path) {
    try {
        processor_.reset(); // Clear any previous state
        std::string body_code = generate_function_body(html_content);
        std::string head_code = processor_.get_head_code();

        // Parse function signature and handle __func__ replacement
        FunctionSignature sig = parse_function_signature(function_signature);

        // Replace __func__ with actual function name if needed
        if (sig.function_name == "__func__") {
            sig.function_name = generate_function_name_from_path(input_file_path);
        }

        // Create complete function declaration
        std::string function_declaration = sig.return_type + " " + sig.function_name + "(" + sig.parameters + ")";

        return wrap_in_header_with_custom_signature(body_code, head_code, function_declaration);
    } catch (const std::exception& e) {
        return "// Error: " + std::string(e.what()) + "\n";
    }
}

FunctionSignature CodeGenerator::parse_function_signature(const std::string& signature) {
    FunctionSignature result;

    // Find the function name by looking for the pattern: type name(params)
    size_t open_paren = signature.find('(');
    if (open_paren == std::string::npos) {
        throw std::runtime_error("Invalid function signature: missing opening parenthesis");
    }

    size_t close_paren = signature.rfind(')');
    if (close_paren == std::string::npos || close_paren <= open_paren) {
        throw std::runtime_error("Invalid function signature: missing or misplaced closing parenthesis");
    }

    // Extract parameters (everything between parentheses)
    result.parameters = signature.substr(open_paren + 1, close_paren - open_paren - 1);

    // Extract return type and function name (everything before opening parenthesis)
    std::string prefix = signature.substr(0, open_paren);

    // Trim whitespace from prefix
    size_t start = 0;
    size_t end = prefix.length();
    while (start < end && std::isspace(prefix[start])) start++;
    while (end > start && std::isspace(prefix[end - 1])) end--;
    prefix = prefix.substr(start, end - start);

    // Find the last space to separate return type and function name
    size_t last_space = prefix.rfind(' ');
    if (last_space == std::string::npos) {
        throw std::runtime_error("Invalid function signature: cannot separate return type and function name");
    }

    result.return_type = prefix.substr(0, last_space);
    result.function_name = prefix.substr(last_space + 1);

    // Trim whitespace from return type
    start = 0;
    end = result.return_type.length();
    while (start < end && std::isspace(result.return_type[start])) start++;
    while (end > start && std::isspace(result.return_type[end - 1])) end--;
    result.return_type = result.return_type.substr(start, end - start);

    return result;
}

std::string CodeGenerator::generate_function_name_from_path(const std::string& file_path) {
    // Extract filename without extension
    size_t last_slash = file_path.find_last_of("/\\");
    size_t last_dot = file_path.find_last_of('.');

    std::string base_name;
    if (last_slash == std::string::npos) {
        // No directory separator found
        if (last_dot == std::string::npos) {
            base_name = file_path;
        } else {
            base_name = file_path.substr(0, last_dot);
        }
    } else {
        // Directory separator found
        if (last_dot == std::string::npos || last_dot < last_slash) {
            base_name = file_path.substr(last_slash + 1);
        } else {
            base_name = file_path.substr(last_slash + 1, last_dot - last_slash - 1);
        }
    }

    // Use only the base filename without directory prefix
    return sanitize_identifier(base_name);
}

std::string CodeGenerator::sanitize_identifier(const std::string& input) {
    std::string result;

    for (char c : input) {
        if (std::isalnum(c)) {
            result += c;
        } else {
            result += '_';
        }
    }

    // Ensure it doesn't start with a digit
    if (!result.empty() && std::isdigit(result[0])) {
        result = "_" + result;
    }

    return result.empty() ? "_unnamed" : result;
}

std::string CodeGenerator::wrap_in_header_with_custom_signature(const std::string& body_code, const std::string& head_code, const std::string& function_declaration) {
    std::ostringstream result;

    result << "#include <iostream>\n";
    result << "#include <string>\n";
    result << "#include <string_view>\n";
    result << "#include <vector>\n";
    result << "#include <sys/uio.h>  // for iovec\n";
    result << "#include \"httpserver/http_response.h\"\n";
    result << "#include \"httpserver/http_util.h\"\n";
    result << "\n";
    result << "using httpserver::HttpResponse;\n";
    result << "using httpserver::util::stringify;\n";
    result << "\n";

    if (!head_code.empty()) {
        result << head_code;
    }

    result << "\n";
    result << function_declaration << " {\n";
    result << body_code;
    result << "}\n";

    return result.str();
}

std::string CodeGenerator::generate_header_with_injection(const std::string& html_content, const std::string& function_signature, const std::string& input_file_path, const std::string& injection_file_path) {
    try {
        // Parse injection file if provided
        InjectionContent injection;
        if (!injection_file_path.empty()) {
            injection = parse_injection_file(injection_file_path);
        }

        processor_.reset(); // Clear any previous state
        std::string body_code = generate_function_body(html_content);
        std::string template_head_code = processor_.get_head_code();
        std::string template_tail_code = processor_.get_tail_code();

        // Parse function signature and handle __func__ replacement
        FunctionSignature sig = parse_function_signature(function_signature);

        // Replace __func__ with actual function name if needed
        if (sig.function_name == "__func__") {
            sig.function_name = generate_function_name_from_path(input_file_path);
        }

        // Create complete function declaration
        std::string function_declaration = sig.return_type + " " + sig.function_name + "(" + sig.parameters + ")";

        return wrap_in_header_with_injection(body_code, template_head_code, template_tail_code, function_declaration, injection);
    } catch (const std::exception& e) {
        return "// Error: " + std::string(e.what()) + "\n";
    }
}

InjectionContent CodeGenerator::parse_injection_file(const std::string& file_path) {
    try {
        // Read the injection file
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open injection file: " + file_path);
        }

        std::ostringstream content;
        content << file.rdbuf();
        std::string injection_html = content.str();

        // Use a separate TemplateProcessor instance for parsing injection file
        TemplateProcessor injection_processor;
        auto blocks = injection_processor.parse_injection_blocks(injection_html);

        InjectionContent result;
        result.head_code = blocks.head_code;
        result.source_code = blocks.source_code;
        result.tail_code = blocks.tail_code;

        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing injection file '" + file_path + "': " + e.what());
    }
}

std::string CodeGenerator::wrap_in_header_with_injection(const std::string& body_code, const std::string& template_head_code, const std::string& template_tail_code, const std::string& function_declaration, const InjectionContent& injection) {
    std::ostringstream result;

    result << "#include <iostream>\n";
    result << "#include <string>\n";
    result << "#include <string_view>\n";
    result << "#include <vector>\n";
    result << "#include <sys/uio.h>  // for iovec\n";
    result << "#include \"httpserver/http_response.h\"\n";
    result << "#include \"httpserver/http_util.h\"\n";
    result << "\n";
    result << "using httpserver::HttpResponse;\n";
    result << "using httpserver::util::stringify;\n";
    result << "\n";

    // 1. Injection head code (外部头部)
    if (!injection.head_code.empty()) {
        result << injection.head_code << "\n";
    }

    // 2. Template head code (模板头部)
    if (!template_head_code.empty()) {
        result << template_head_code << "\n";
    }

    result << "\n";
    result << function_declaration << " {\n";

    // 3. Injection source code (外部源码)
    if (!injection.source_code.empty()) {
        // Add proper indentation to each line
        std::istringstream iss(injection.source_code);
        std::string line;
        while (std::getline(iss, line)) {
            result << "    " << line << "\n";
        }
    }

    // 4. Template body code (模板内容和内部x-cpp-source)
    result << body_code;

    // 5. Injection tail code (外部尾部)
    if (!injection.tail_code.empty()) {
        // Add proper indentation to each line
        std::istringstream iss(injection.tail_code);
        std::string line;
        while (std::getline(iss, line)) {
            result << "    " << line << "\n";
        }
    }

    // 6. Template tail code (模板尾部)
    if (!template_tail_code.empty()) {
        // Add proper indentation to each line
        std::istringstream iss(template_tail_code);
        std::string line;
        while (std::getline(iss, line)) {
            result << "    " << line << "\n";
        }
    }

    result << "}\n";

    return result.str();
}

} // namespace weave
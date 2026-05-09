#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <lexbor/dom/dom.h>
#include "lexbor_wrapper.hpp"

namespace weave {

class TemplateProcessingError : public std::runtime_error {
public:
    TemplateProcessingError(const std::string& message) : std::runtime_error(message) {}
};

struct TemplateDirective {
    enum Type {
        NONE,
        TEXT,      // cpp-text
        IF,        // cpp-if
        FOR,       // cpp-for
        ATTR,      // cpp:*
        SCRIPT_SOURCE,  // <script type="application/x-cpp-source">
        SCRIPT_HEAD,    // <script type="application/x-cpp-head">
        SCRIPT_TAIL     // <script type="application/x-cpp-tail">
    };

    Type type = NONE;
    std::string expression;
    std::string attribute_name; // for ATTR type
};

// 向量化I/O输出片段
struct OutputSegment {
    enum Type {
        STATIC_STRING,    // 静态字符串常量
        DYNAMIC_EXPR,     // 动态表达式
        CODE_BLOCK        // 代码块
    };

    Type type;
    std::string content;

    OutputSegment(Type t, const std::string& c) : type(t), content(c) {}
};

class TemplateProcessor {
public:
    std::string process_node(lxb_dom_node_t* node);
    std::string preprocess_html(const std::string& html);

    // 向量化I/O处理方法
    std::vector<OutputSegment> process_node_vectorized(lxb_dom_node_t* node);

    // Get collected head code blocks
    const std::string& get_head_code() const { return head_code_; }

    // Get collected tail code blocks
    const std::string& get_tail_code() const { return tail_code_; }

    // Reset state for new processing
    void reset() { head_code_.clear(); tail_code_.clear(); }

    // Parse injection file and extract code blocks
    struct InjectionBlocks {
        std::string head_code;
        std::string source_code;
        std::string tail_code;
    };
    InjectionBlocks parse_injection_blocks(const std::string& html_content);

private:
    TemplateDirective parse_directives(lxb_dom_element_t* element);
    std::string escape_string(const std::string& str);
    std::string process_element(lxb_dom_element_t* element);
    std::string process_text_node(lxb_dom_text_t* text);
    std::string get_tag_name(lxb_dom_element_t* element);
    std::string get_attributes(lxb_dom_element_t* element, const TemplateDirective& directive);
    std::string get_element_content(lxb_dom_element_t* element, const TemplateDirective& directive);
    
    // Script tag processing
    std::string process_script_element(lxb_dom_element_t* element);
    std::string get_script_type(lxb_dom_element_t* element);
    std::string get_element_text_content(lxb_dom_element_t* element);

    // 向量化I/O处理的私有方法
    std::vector<OutputSegment> process_element_vectorized(lxb_dom_element_t* element);
    std::vector<OutputSegment> process_text_node_vectorized(lxb_dom_text_t* text);
    std::vector<OutputSegment> parse_interpolations_vectorized(const std::string& text);
    std::vector<OutputSegment> process_script_element_vectorized(lxb_dom_element_t* element);

    // Storage for head and tail code blocks
    std::string head_code_;
    std::string tail_code_;
    
    // Legacy syntax support
    bool is_code_block(const std::string& text);
    std::string extract_code_block(const std::string& text);
    std::string process_interpolations(const std::string& text);
    bool contains_interpolation(const std::string& text);
    
    struct InterpolationResult {
        std::string processed_text;
        bool has_interpolation;
    };
};

} // namespace weave
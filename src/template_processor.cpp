#include "template_processor.hpp"
#include <sstream>
#include <cstring>
#include <iostream>
#include <vector>

namespace weave {

std::string TemplateProcessor::process_node(lxb_dom_node_t* node) {
    if (!node) return "";

    switch (node->type) {
        case LXB_DOM_NODE_TYPE_ELEMENT:
            return process_element(lxb_dom_interface_element(node));
        case LXB_DOM_NODE_TYPE_TEXT:
            return process_text_node(lxb_dom_interface_text(node));
        default:
            return "";
    }
}

std::string TemplateProcessor::process_element(lxb_dom_element_t* element) {
    std::ostringstream result;
    
    std::string tag_name = get_tag_name(element);
    
    // Handle <script> tags specially
    if (tag_name == "script") {
        return process_script_element(element);
    }
    
    TemplateDirective directive = parse_directives(element);
    std::string attributes = get_attributes(element, directive);
    std::string content = get_element_content(element, directive);

    // Handle cpp-if directive
    if (directive.type == TemplateDirective::IF) {
        result << "if (" << directive.expression << ") {\n";
    }
    
    // Handle cpp-for directive
    if (directive.type == TemplateDirective::FOR) {
        result << "for (" << directive.expression << ") {\n";
    }

    // Generate opening tag
    if (directive.type == TemplateDirective::TEXT) {
        if (attributes.find("<<") != std::string::npos) {
            result << "out << \"<" << tag_name << attributes << ">\" << (" 
                   << directive.expression << ") << \"</" << tag_name << ">\";\n";
        } else {
            result << "out << \"<" << tag_name << attributes << ">\" << (" 
                   << directive.expression << ") << \"</" << tag_name << ">\";\n";
        }
    } else {
        if (attributes.find("<<") != std::string::npos) {
            // Dynamic attributes - need to close tag properly
            result << "out << \"<" << tag_name << attributes << ">\";\n";
        } else {
            result << "out << \"<" << tag_name << attributes << ">\";\n";
        }
        result << content;
        result << "out << \"</" << tag_name << ">\";\n";
    }

    // Close control flow blocks
    if (directive.type == TemplateDirective::IF || directive.type == TemplateDirective::FOR) {
        result << "}\n";
    }

    return result.str();
}

std::string TemplateProcessor::process_text_node(lxb_dom_text_t* text) {
    if (!text) return "";

    size_t length;
    const lxb_char_t* data = lxb_dom_node_text_content(lxb_dom_interface_node(text), &length);
    if (!data || length == 0) return "";

    std::string text_content(reinterpret_cast<const char*>(data), length);
    
    // Check if this is a code execution block <% ... %>
    if (is_code_block(text_content)) {
        return extract_code_block(text_content) + "\n";
    }
    
    // 保留包含换行和缩进的空白节点，但跳过完全空的节点
    if (text_content.empty()) {
        return "";
    }

    // 如果是纯空白但包含换行符，则保留（为了维持HTML格式）
    bool has_newline = text_content.find('\n') != std::string::npos;
    bool is_pure_spaces_tabs = true;
    for (char c : text_content) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            is_pure_spaces_tabs = false;
            break;
        }
    }

    // 跳过只有空格和制表符、但没有换行符的节点（通常是元素间的无意义空白）
    if (is_pure_spaces_tabs && !has_newline) {
        return "";
    }

    // Process interpolations {{...}} in text content
    std::string processed = process_interpolations(text_content);
    if (processed.empty()) {
        return "";
    }
    
    return processed;
}

TemplateDirective TemplateProcessor::parse_directives(lxb_dom_element_t* element) {
    TemplateDirective directive;
    
    // Priority: cpp-text > cpp-if > cpp-for > cpp:*
    lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element);
    while (attr) {
        size_t name_length;
        const lxb_char_t* attr_name = lxb_dom_attr_qualified_name(attr, &name_length);
        
        if (attr_name) {
            std::string name(reinterpret_cast<const char*>(attr_name), name_length);
            
            if (name == "cpp-text") {
                directive.type = TemplateDirective::TEXT;
                size_t value_length;
                const lxb_char_t* attr_value = lxb_dom_attr_value(attr, &value_length);
                if (attr_value) {
                    directive.expression = std::string(reinterpret_cast<const char*>(attr_value), value_length);
                }
                return directive; // cpp-text has highest priority
            }
        }
        
        attr = lxb_dom_element_next_attribute(attr);
    }
    
    // Check for cpp-if
    attr = lxb_dom_element_first_attribute(element);
    while (attr) {
        size_t name_length;
        const lxb_char_t* attr_name = lxb_dom_attr_qualified_name(attr, &name_length);
        
        if (attr_name) {
            std::string name(reinterpret_cast<const char*>(attr_name), name_length);
            
            if (name == "cpp-if") {
                directive.type = TemplateDirective::IF;
                size_t value_length;
                const lxb_char_t* attr_value = lxb_dom_attr_value(attr, &value_length);
                if (attr_value) {
                    directive.expression = std::string(reinterpret_cast<const char*>(attr_value), value_length);
                }
                return directive;
            }
        }
        
        attr = lxb_dom_element_next_attribute(attr);
    }
    
    // Check for cpp-for
    attr = lxb_dom_element_first_attribute(element);
    while (attr) {
        size_t name_length;
        const lxb_char_t* attr_name = lxb_dom_attr_qualified_name(attr, &name_length);
        
        if (attr_name) {
            std::string name(reinterpret_cast<const char*>(attr_name), name_length);
            
            if (name == "cpp-for") {
                directive.type = TemplateDirective::FOR;
                size_t value_length;
                const lxb_char_t* attr_value = lxb_dom_attr_value(attr, &value_length);
                if (attr_value) {
                    directive.expression = std::string(reinterpret_cast<const char*>(attr_value), value_length);
                }
                return directive;
            }
        }
        
        attr = lxb_dom_element_next_attribute(attr);
    }
    
    return directive;
}

std::string TemplateProcessor::get_tag_name(lxb_dom_element_t* element) {
    size_t name_length;
    const lxb_char_t* tag_name = lxb_dom_element_qualified_name(element, &name_length);
    if (tag_name) {
        return std::string(reinterpret_cast<const char*>(tag_name), name_length);
    }
    return "";
}

std::string TemplateProcessor::get_attributes(lxb_dom_element_t* element, const TemplateDirective& /* directive */) {
    std::ostringstream attrs;
    bool has_dynamic_attr = false;
    
    lxb_dom_attr_t* attr = lxb_dom_element_first_attribute(element);
    while (attr) {
        size_t name_length;
        const lxb_char_t* attr_name = lxb_dom_attr_qualified_name(attr, &name_length);
        
        if (attr_name) {
            std::string name(reinterpret_cast<const char*>(attr_name), name_length);
            
            // Skip cpp-* directives
            if (name.substr(0, 4) == "cpp-") {
                // Skip all cpp- directives
            } else if (name.substr(0, 4) == "cpp:") {
                // Handle dynamic attribute - need to close current string and append expression
                std::string attr_name_clean = name.substr(4);
                size_t value_length;
                const lxb_char_t* attr_value = lxb_dom_attr_value(attr, &value_length);
                if (attr_value) {
                    std::string expression(reinterpret_cast<const char*>(attr_value), value_length);
                    // Close the current string, add the attribute with expression
                    attrs << " " << attr_name_clean << "=\\\"\" << (" 
                          << expression << ") << \"\\\"";
                    has_dynamic_attr = true;
                }
            } else {
                // Regular attribute - check for interpolations
                size_t value_length;
                const lxb_char_t* attr_value = lxb_dom_attr_value(attr, &value_length);
                attrs << " " << name;
                if (attr_value && value_length > 0) {
                    std::string value(reinterpret_cast<const char*>(attr_value), value_length);
                    
                    // Check if attribute value contains interpolations
                    if (contains_interpolation(value)) {
                        // Process interpolations and create dynamic attribute
                        std::string processed = process_interpolations(value);
                        // Remove the "out << " prefix and ";\n" suffix for attribute context
                        if (processed.substr(0, 7) == "out << ") {
                            processed = processed.substr(7);
                        }
                        if (processed.substr(processed.length() - 2) == ";\n") {
                            processed = processed.substr(0, processed.length() - 2);
                        }
                        attrs << "=\\\"\" << " << processed << " << \"\\\"";
                        has_dynamic_attr = true;
                    } else {
                        // 静态属性：直接输出原始HTML格式，不转义
                        // CodeGenerator::escape_string 会稍后处理转义
                        attrs << "=\"" << value << "\"";
                    }
                }
            }
        }
        
        attr = lxb_dom_element_next_attribute(attr);
    }
    
    std::string result = attrs.str();
    if (has_dynamic_attr) {
        // Return with proper quoting for dynamic attributes
        return result;
    }
    return result;
}

std::string TemplateProcessor::get_element_content(lxb_dom_element_t* element, const TemplateDirective& directive) {
    if (directive.type == TemplateDirective::TEXT) {
        return ""; // Content replaced by expression
    }
    
    std::ostringstream content;
    lxb_dom_node_t* child = lxb_dom_node_first_child(lxb_dom_interface_node(element));
    
    while (child) {
        content << process_node(child);
        child = lxb_dom_node_next(child);
    }
    
    return content.str();
}

std::string TemplateProcessor::escape_string(const std::string& str) {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
            case '"':  escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\n': escaped << "\\n"; break;
            case '\t': escaped << "\\t"; break;
            case '\r': escaped << "\\r"; break;
            default:   escaped << c; break;
        }
    }
    return escaped.str();
}

bool TemplateProcessor::is_code_block(const std::string& text) {
    // Trim whitespace
    size_t start = 0, end = text.length();
    while (start < end && std::isspace(text[start])) start++;
    while (end > start && std::isspace(text[end - 1])) end--;
    
    if (end - start < 4) return false; // Minimum: "<%%>"
    
    return text.substr(start, 2) == "<%" && text.substr(end - 2, 2) == "%>";
}

std::string TemplateProcessor::extract_code_block(const std::string& text) {
    // Trim whitespace
    size_t start = 0, end = text.length();
    while (start < end && std::isspace(text[start])) start++;
    while (end > start && std::isspace(text[end - 1])) end--;
    
    if (!is_code_block(text)) return "";
    
    // Extract content between <% and %>
    std::string code = text.substr(start + 2, end - start - 4);
    
    // Trim whitespace from extracted code
    size_t code_start = 0, code_end = code.length();
    while (code_start < code_end && std::isspace(code[code_start])) code_start++;
    while (code_end > code_start && std::isspace(code[code_end - 1])) code_end--;
    
    return code.substr(code_start, code_end - code_start);
}

bool TemplateProcessor::contains_interpolation(const std::string& text) {
    size_t pos = 0;
    while ((pos = text.find("{{", pos)) != std::string::npos) {
        size_t end = text.find("}}", pos + 2);
        if (end != std::string::npos) {
            return true;
        }
        pos += 2;
    }
    return false;
}

std::string TemplateProcessor::process_interpolations(const std::string& text) {
    if (!contains_interpolation(text)) {
        // No interpolations, return as regular escaped string
        return "out << \"" + escape_string(text) + "\";\n";
    }
    
    std::ostringstream result;
    result << "out << ";
    
    size_t pos = 0;
    bool first = true;
    
    while (pos < text.length()) {
        size_t start = text.find("{{", pos);
        
        if (start == std::string::npos) {
            // No more interpolations, add remaining text
            if (pos < text.length()) {
                if (!first) result << " << ";
                std::string remaining = text.substr(pos);
                result << "\"" << escape_string(remaining) << "\"";
            }
            break;
        }
        
        // Add text before interpolation
        if (start > pos) {
            if (!first) result << " << ";
            std::string before = text.substr(pos, start - pos);
            result << "\"" << escape_string(before) << "\"";
            first = false;
        }
        
        // Find end of interpolation
        size_t end = text.find("}}", start + 2);
        if (end == std::string::npos) {
            // Malformed interpolation - treat remaining as literal text
            if (!first) result << " << ";
            std::string remaining = text.substr(start);
            result << "\"" << escape_string(remaining) << "\"";
            break;
        }
        
        // Extract and validate expression
        std::string expr = text.substr(start + 2, end - start - 2);
        
        // Trim whitespace from expression
        size_t expr_start = 0, expr_end = expr.length();
        while (expr_start < expr_end && std::isspace(expr[expr_start])) expr_start++;
        while (expr_end > expr_start && std::isspace(expr[expr_end - 1])) expr_end--;
        expr = expr.substr(expr_start, expr_end - expr_start);
        
        if (expr.empty()) {
            throw TemplateProcessingError("Empty interpolation expression");
        }
        
        if (!first) result << " << ";
        result << "(" << expr << ")";
        first = false;
        
        pos = end + 2;
    }
    
    result << ";\n";
    return result.str();
}

std::string TemplateProcessor::process_script_element(lxb_dom_element_t* element) {
    std::string script_type = get_script_type(element);
    std::string content = get_element_text_content(element);
    
    if (script_type == "application/x-cpp-source") {
        // Source code inline block - insert directly into function body
        if (!content.empty()) {
            return content + "\n";
        }
        return "";
    }
    else if (script_type == "application/x-cpp-head") {
        // Head code block - collect for file header
        if (!content.empty()) {
            if (!head_code_.empty()) {
                head_code_ += "\n";
            }
            head_code_ += content;
        }
        return ""; // Don't output anything inline
    }
    else if (script_type == "application/x-cpp-tail") {
        // Tail code block - collect for function end
        if (!content.empty()) {
            if (!tail_code_.empty()) {
                tail_code_ += "\n";
            }
            tail_code_ += content;
        }
        return ""; // Don't output anything inline
    } 
    else {
        // Regular script tag - output as HTML
        std::ostringstream result;
        std::string attributes = get_attributes(element, TemplateDirective());
        result << "out << \"<script" << attributes << ">\";\n";
        if (!content.empty()) {
            result << "out << \"" << escape_string(content) << "\";\n";
        }
        result << "out << \"</script>\";\n";
        return result.str();
    }
}

std::string TemplateProcessor::get_script_type(lxb_dom_element_t* element) {
    lxb_dom_attr_t* type_attr = lxb_dom_element_attr_by_name(element, 
        reinterpret_cast<const lxb_char_t*>("type"), 4);
    
    if (type_attr) {
        size_t length;
        const lxb_char_t* value = lxb_dom_attr_value(type_attr, &length);
        if (value && length > 0) {
            return std::string(reinterpret_cast<const char*>(value), length);
        }
    }
    
    return ""; // Default to empty if no type attribute
}

std::string TemplateProcessor::get_element_text_content(lxb_dom_element_t* element) {
    size_t length;
    const lxb_char_t* text = lxb_dom_node_text_content(
        lxb_dom_interface_node(element), &length);
    
    if (!text || length == 0) {
        return "";
    }
    
    std::string content(reinterpret_cast<const char*>(text), length);
    
    // Trim leading and trailing whitespace
    size_t start = 0, end = content.length();
    while (start < end && std::isspace(content[start])) start++;
    while (end > start && std::isspace(content[end - 1])) end--;
    
    return content.substr(start, end - start);
}

std::string TemplateProcessor::preprocess_html(const std::string& html) {
    std::ostringstream result;
    size_t pos = 0;
    
    while (pos < html.length()) {
        // Look for script tags
        size_t script_start = html.find("<script", pos);
        
        if (script_start == std::string::npos) {
            // No more script tags, append rest of HTML
            result << html.substr(pos);
            break;
        }
        
        // Add HTML before the script tag
        result << html.substr(pos, script_start - pos);
        
        // Find end of opening script tag
        size_t tag_end = html.find('>', script_start);
        if (tag_end == std::string::npos) {
            // Malformed - append rest and break
            result << html.substr(script_start);
            break;
        }
        
        // Extract the opening tag
        std::string opening_tag = html.substr(script_start, tag_end - script_start + 1);
        
        // Check if this is one of our special script types
        bool is_cpp_source = opening_tag.find("application/x-cpp-source") != std::string::npos;
        bool is_cpp_head = opening_tag.find("application/x-cpp-head") != std::string::npos;
        
        // Find closing script tag
        size_t script_end = html.find("</script>", tag_end + 1);
        if (script_end == std::string::npos) {
            // Malformed - treat as regular HTML
            result << html.substr(script_start);
            break;
        }
        
        if (is_cpp_source || is_cpp_head) {
            // Extract content between tags
            std::string content = html.substr(tag_end + 1, script_end - tag_end - 1);
            
            // Trim whitespace
            size_t start = 0, end = content.length();
            while (start < end && std::isspace(content[start])) start++;
            while (end > start && std::isspace(content[end - 1])) end--;
            content = content.substr(start, end - start);
            
            if (is_cpp_head) {
                // Add to head code
                if (!head_code_.empty()) {
                    head_code_ += "\n";
                }
                head_code_ += content;
            } else if (is_cpp_source) {
                // Convert to code block format for processing
                result << "<% " << content << " %>";
            }
            
            // Skip the script tag entirely
            pos = script_end + 9; // length of </script>
        } else {
            // Regular script tag - include as is
            result << html.substr(script_start, script_end + 9 - script_start);
            pos = script_end + 9;
        }
    }
    
    return result.str();
}

std::vector<OutputSegment> TemplateProcessor::process_node_vectorized(lxb_dom_node_t* node) {
    if (!node) return {};

    switch (node->type) {
        case LXB_DOM_NODE_TYPE_ELEMENT:
            return process_element_vectorized(lxb_dom_interface_element(node));
        case LXB_DOM_NODE_TYPE_TEXT:
            return process_text_node_vectorized(lxb_dom_interface_text(node));
        default:
            return {};
    }
}

std::vector<OutputSegment> TemplateProcessor::process_element_vectorized(lxb_dom_element_t* element) {
    std::vector<OutputSegment> result;

    std::string tag_name = get_tag_name(element);

    // Handle <script> tags specially
    if (tag_name == "script") {
        return process_script_element_vectorized(element);
    }

    TemplateDirective directive = parse_directives(element);
    std::string attributes = get_attributes(element, directive);

    // Handle control flow directives
    if (directive.type == TemplateDirective::IF) {
        result.emplace_back(OutputSegment::CODE_BLOCK, "if (" + directive.expression + ") {");
    } else if (directive.type == TemplateDirective::FOR) {
        result.emplace_back(OutputSegment::CODE_BLOCK, "for (" + directive.expression + ") {");
    }

    // Generate opening tag
    if (directive.type == TemplateDirective::TEXT) {
        // For cpp-text, we output the tag with the expression as content
        std::string opening = "<" + tag_name + attributes + ">";
        std::string closing = "</" + tag_name + ">";
        result.emplace_back(OutputSegment::STATIC_STRING, opening);
        result.emplace_back(OutputSegment::DYNAMIC_EXPR, directive.expression);
        result.emplace_back(OutputSegment::STATIC_STRING, closing);
    } else {
        // Regular element processing
        std::string opening = "<" + tag_name + attributes + ">";
        result.emplace_back(OutputSegment::STATIC_STRING, opening);

        // Process child nodes
        lxb_dom_node_t* child = lxb_dom_node_first_child(lxb_dom_interface_node(element));
        while (child) {
            auto child_segments = process_node_vectorized(child);
            result.insert(result.end(), child_segments.begin(), child_segments.end());
            child = lxb_dom_node_next(child);
        }

        std::string closing = "</" + tag_name + ">";
        result.emplace_back(OutputSegment::STATIC_STRING, closing);
    }

    // Close control flow blocks
    if (directive.type == TemplateDirective::IF || directive.type == TemplateDirective::FOR) {
        result.emplace_back(OutputSegment::CODE_BLOCK, "}");
    }

    return result;
}

std::vector<OutputSegment> TemplateProcessor::process_text_node_vectorized(lxb_dom_text_t* text) {
    if (!text) return {};

    size_t length;
    const lxb_char_t* data = lxb_dom_node_text_content(lxb_dom_interface_node(text), &length);
    if (!data || length == 0) return {};

    std::string text_content(reinterpret_cast<const char*>(data), length);

    // Check if this is a code execution block <% ... %>
    if (is_code_block(text_content)) {
        std::string code = extract_code_block(text_content);
        return {OutputSegment(OutputSegment::CODE_BLOCK, code)};
    }

    // 保留包含换行和缩进的空白节点，但跳过完全空的节点
    if (text_content.empty()) {
        return {};
    }

    // 如果是纯空白但包含换行符，则保留（为了维持HTML格式）
    bool has_newline = text_content.find('\n') != std::string::npos;
    bool is_pure_spaces_tabs = true;
    for (char c : text_content) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            is_pure_spaces_tabs = false;
            break;
        }
    }

    // 跳过只有空格和制表符、但没有换行符的节点（通常是元素间的无意义空白）
    if (is_pure_spaces_tabs && !has_newline) {
        return {};
    }

    // Process interpolations {{...}} in text content
    return parse_interpolations_vectorized(text_content);
}

std::vector<OutputSegment> TemplateProcessor::parse_interpolations_vectorized(const std::string& text) {
    std::vector<OutputSegment> result;

    if (!contains_interpolation(text)) {
        // No interpolations, return as single static string
        result.emplace_back(OutputSegment::STATIC_STRING, text);
        return result;
    }

    size_t pos = 0;

    while (pos < text.length()) {
        size_t start = text.find("{{", pos);

        if (start == std::string::npos) {
            // No more interpolations, add remaining text
            if (pos < text.length()) {
                std::string remaining = text.substr(pos);
                result.emplace_back(OutputSegment::STATIC_STRING, remaining);
            }
            break;
        }

        // Add text before interpolation
        if (start > pos) {
            std::string before = text.substr(pos, start - pos);
            result.emplace_back(OutputSegment::STATIC_STRING, before);
        }

        // Find end of interpolation
        size_t end = text.find("}}", start + 2);
        if (end == std::string::npos) {
            // Malformed interpolation - treat remaining as literal text
            std::string remaining = text.substr(start);
            result.emplace_back(OutputSegment::STATIC_STRING, remaining);
            break;
        }

        // Extract and validate expression
        std::string expr = text.substr(start + 2, end - start - 2);

        // Trim whitespace from expression
        size_t expr_start = 0, expr_end = expr.length();
        while (expr_start < expr_end && std::isspace(expr[expr_start])) expr_start++;
        while (expr_end > expr_start && std::isspace(expr[expr_end - 1])) expr_end--;
        expr = expr.substr(expr_start, expr_end - expr_start);

        if (expr.empty()) {
            throw TemplateProcessingError("Empty interpolation expression");
        }

        result.emplace_back(OutputSegment::DYNAMIC_EXPR, expr);

        pos = end + 2;
    }

    return result;
}

std::vector<OutputSegment> TemplateProcessor::process_script_element_vectorized(lxb_dom_element_t* element) {
    std::vector<OutputSegment> result;

    std::string script_type = get_script_type(element);
    std::string content = get_element_text_content(element);

    if (script_type == "application/x-cpp-source") {
        // Source code inline block - insert directly into function body
        if (!content.empty()) {
            result.emplace_back(OutputSegment::CODE_BLOCK, content);
        }
        return result;
    }
    else if (script_type == "application/x-cpp-head") {
        // Head code block - collect for file header
        if (!content.empty()) {
            if (!head_code_.empty()) {
                head_code_ += "\n";
            }
            head_code_ += content;
        }
        return result; // Return empty - don't output anything inline
    }
    else if (script_type == "application/x-cpp-tail") {
        // Tail code block - collect for function end
        if (!content.empty()) {
            if (!tail_code_.empty()) {
                tail_code_ += "\n";
            }
            tail_code_ += content;
        }
        return result; // Return empty - don't output anything inline
    }
    else {
        // Regular HTML script tag - output as HTML
        std::string attributes = get_attributes(element, TemplateDirective());
        std::string opening = "<script" + attributes + ">";
        std::string closing = "</script>";

        result.emplace_back(OutputSegment::STATIC_STRING, opening);
        if (!content.empty()) {
            result.emplace_back(OutputSegment::STATIC_STRING, content);
        }
        result.emplace_back(OutputSegment::STATIC_STRING, closing);

        return result;
    }
}

TemplateProcessor::InjectionBlocks TemplateProcessor::parse_injection_blocks(const std::string& html_content) {
    InjectionBlocks result;

    try {
        // Don't use preprocess_html for injection files as it removes script tags
        LexborDocument doc(html_content);
        lxb_dom_node_t* root = doc.body();

        if (root) {
            // Use a recursive function to find all script elements
            std::function<void(lxb_dom_node_t*)> process_node_recursive;
            process_node_recursive = [&](lxb_dom_node_t* node) {
                if (!node) return;

                if (node->type == LXB_DOM_NODE_TYPE_ELEMENT) {
                    lxb_dom_element_t* element = lxb_dom_interface_element(node);
                    std::string tag_name = get_tag_name(element);

                    if (tag_name == "script") {
                        std::string script_type = get_script_type(element);
                        std::string script_content = get_element_text_content(element);


                        if (script_type == "application/x-cpp-head") {
                            if (!script_content.empty()) {
                                if (!result.head_code.empty()) {
                                    result.head_code += "\n";
                                }
                                result.head_code += script_content;
                            }
                        }
                        else if (script_type == "application/x-cpp-source") {
                            if (!script_content.empty()) {
                                if (!result.source_code.empty()) {
                                    result.source_code += "\n";
                                }
                                result.source_code += script_content;
                            }
                        }
                        else if (script_type == "application/x-cpp-tail") {
                            if (!script_content.empty()) {
                                if (!result.tail_code.empty()) {
                                    result.tail_code += "\n";
                                }
                                result.tail_code += script_content;
                            }
                        }
                    }
                }

                // Recursively process child nodes
                lxb_dom_node_t* child = lxb_dom_node_first_child(node);
                while (child) {
                    process_node_recursive(child);
                    child = lxb_dom_node_next(child);
                }
            };

            process_node_recursive(root);
        }
    } catch (const std::exception& e) {
        throw TemplateProcessingError("Error parsing injection blocks: " + std::string(e.what()));
    }

    return result;
}

} // namespace weave
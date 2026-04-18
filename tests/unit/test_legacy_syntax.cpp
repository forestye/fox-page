#include "src/code_generator.hpp"
#include <iostream>
#include <cassert>

void test_code_blocks() {
    std::cout << "Testing code execution blocks...\n";
    
    weave::CodeGenerator generator;
    
    // Test basic code block
    std::string html1 = "<div><% int x = 42; %></div>";
    std::string result1 = generator.generate(html1);
    std::cout << "Code block test:\n" << result1 << "\n";
    
    // Test code block with cpp-if
    std::string html2 = "<div cpp-if=\"user.isActive()\"><% auto userData = user.getData(); %><p>Active user</p></div>";
    std::string result2 = generator.generate(html2);
    std::cout << "Code block with cpp-if:\n" << result2 << "\n";
}

void test_interpolations() {
    std::cout << "Testing interpolations...\n";
    
    weave::CodeGenerator generator;
    
    // Test basic interpolation
    std::string html1 = "<p>Hello, {{user.getName()}}!</p>";
    std::string result1 = generator.generate(html1);
    std::cout << "Basic interpolation:\n" << result1 << "\n";
    
    // Test multiple interpolations
    std::string html2 = "<p>User: {{user.getName()}}, Age: {{user.getAge()}}</p>";
    std::string result2 = generator.generate(html2);
    std::cout << "Multiple interpolations:\n" << result2 << "\n";
    
    // Test interpolation in attributes
    std::string html3 = R"(<div class="user-card {{user.getThemeClass()}}" id="user-{{user.getId()}}">Content</div>)";
    std::string result3 = generator.generate(html3);
    std::cout << "Attribute interpolations:\n" << result3 << "\n";
}

void test_mixed_syntax() {
    std::cout << "Testing mixed legacy and cpp-* syntax...\n";
    
    weave::CodeGenerator generator;
    
    std::string html = "<div class=\"account-summary\" cpp-if=\"context.user.isLoggedIn()\">"
                      "<% Account account = getAllAccount(context.user.getId()); %>"
                      "<h2>Account Details</h2>"
                      "<div class=\"account-card {{status_class}}\">"
                      "<p>Holder: {{context.user.getFullName()}}</p>"
                      "<div class=\"balance\">"
                      "<span>Balance:</span>"
                      "<strong>${{account.getBalance()}}</strong>"
                      "</div>"
                      "</div>"
                      "</div>";
    
    std::string result = generator.generate(html);
    std::cout << "Mixed syntax test:\n" << result << "\n";
}

void test_error_handling() {
    std::cout << "Testing error handling...\n";
    
    weave::CodeGenerator generator;
    
    // Test unclosed interpolation
    std::string html1 = "<p>Hello, {{user.getName()!</p>";
    std::string result1 = generator.generate(html1);
    if (result1.find("Error:") != std::string::npos) {
        std::cout << "Correctly caught unclosed interpolation error\n";
    } else {
        std::cout << "ERROR: Should have detected unclosed interpolation\n";
    }
    
    // Test empty interpolation
    std::string html2 = "<p>Hello, {{}}!</p>";
    std::string result2 = generator.generate(html2);
    if (result2.find("Error:") != std::string::npos) {
        std::cout << "Correctly caught empty interpolation error\n";
    } else {
        std::cout << "ERROR: Should have detected empty interpolation\n";
    }
    
    // Test normal case works
    std::string html3 = "<p>Hello, {{user.getName()}}!</p>";
    std::string result3 = generator.generate(html3);
    if (result3.find("Error:") == std::string::npos) {
        std::cout << "Normal case works correctly\n";
    } else {
        std::cout << "ERROR: Normal case should not produce error\n";
    }
}

int main() {
    std::cout << "=== Weave++ Legacy Syntax Tests ===\n\n";
    
    test_code_blocks();
    std::cout << "\n";
    
    test_interpolations();
    std::cout << "\n";
    
    test_mixed_syntax();
    std::cout << "\n";
    
    test_error_handling();
    std::cout << "\n";
    
    std::cout << "=== All tests completed ===\n";
    
    return 0;
}
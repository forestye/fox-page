#include "cli.hpp"
#include "code_generator.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

std::string read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void write_file(const std::string& filepath, const std::string& content) {
    // 确保父目录存在
    fs::path p(filepath);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + filepath);
    }

    file << content;
}

// 判断 -o 参数指定的是目录还是文件
bool is_output_directory(const std::string& output_spec) {
    fs::path p(output_spec);

    // 规则1：如果路径已存在，以现状为准
    if (fs::exists(p)) {
        return fs::is_directory(p);
    }

    // 规则2：如果以 / 结尾，则为目录
    if (!output_spec.empty() && output_spec.back() == '/') {
        return true;
    }

    // 规则3：如果以 .cpp 结尾，则为文件
    if (output_spec.length() >= 4 && output_spec.substr(output_spec.length() - 4) == ".cpp") {
        return false;
    }

    // 规则4：其余情况均认为是文件
    return false;
}

std::string get_output_filename(const std::string& input_path, const std::string& output_spec) {
    fs::path input(input_path);
    std::string basename = input.stem().string();

    if (is_output_directory(output_spec)) {
        // 作为目录处理：output_spec/basename.cpp
        fs::path output_path = fs::path(output_spec) / (basename + ".cpp");
        return output_path.string();
    } else {
        // 作为文件处理：直接使用 output_spec
        return output_spec;
    }
}

std::string get_function_name(const std::string& input_path) {
    fs::path input(input_path);
    std::string basename = input.stem().string();
    
    // Convert filename to valid C++ function name
    std::string func_name = "render_" + basename;
    
    // Replace non-alphanumeric characters with underscore
    for (char& c : func_name) {
        if (!std::isalnum(c)) {
            c = '_';
        }
    }
    
    return func_name;
}

int main(int argc, char* argv[]) {
    try {
        weave::CliParser parser;
        weave::CliOptions options = parser.parse(argc, argv);
        
        if (options.show_help) {
            parser.print_help();
            return 0;
        }
        
        if (options.show_version) {
            parser.print_version();
            return 0;
        }
        
        if (options.input_files.empty()) {
            std::cerr << "Error: No input files specified\n";
            parser.print_help();
            return 1;
        }
        
        // 如果有多个输入文件，-o 必须是目录
        if (options.input_files.size() > 1 && !is_output_directory(options.output_directory)) {
            std::cerr << "Error: When processing multiple files, -o must specify a directory" << std::endl;
            std::cerr << "Hint: Use -o dirname/ or ensure the path is an existing directory" << std::endl;
            return 1;
        }

        // 如果 -o 是目录且不存在，则创建目录
        if (is_output_directory(options.output_directory) && !fs::exists(options.output_directory)) {
            if (!fs::create_directories(options.output_directory)) {
                std::cerr << "Error: Cannot create output directory: "
                          << options.output_directory << std::endl;
                return 1;
            }
        }

        weave::CodeGenerator generator;

        for (const std::string& input_file : options.input_files) {
            try {
                std::cout << "Processing: " << input_file << std::endl;
                
                // Read input file
                std::string html_content = read_file(input_file);

                // Generate complete C++ file using custom function signature and injection
                std::string cpp_code = generator.generate_header_with_injection(
                    html_content,
                    options.function_signature,
                    input_file,
                    options.extra_file
                );
                
                // Write output file
                std::string output_file = get_output_filename(input_file, options.output_directory);
                write_file(output_file, cpp_code);
                
                std::cout << "Generated: " << output_file << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "Error processing " << input_file << ": " 
                          << e.what() << std::endl;
                return 1;
            }
        }
        
        std::cout << "Transpilation completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
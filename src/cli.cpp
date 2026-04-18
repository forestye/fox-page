#include "cli.hpp"
#include <iostream>
#include <filesystem>

namespace weave {

CliOptions CliParser::parse(int argc, char* argv[]) {
    CliOptions options;
    
    if (argc > 0) {
        program_name_ = std::filesystem::path(argv[0]).filename();
    }
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
        } else if (arg == "--version" || arg == "-v") {
            options.show_version = true;
        } else if (arg == "-o" && i + 1 < argc) {
            options.output_directory = argv[++i];
        } else if (arg == "--func" && i + 1 < argc) {
            options.function_signature = argv[++i];
        } else if (arg == "--extra" && i + 1 < argc) {
            options.extra_file = argv[++i];
        } else if (arg.front() != '-') {
            // 任何不以'-'开头的参数都视为输入文件
            options.input_files.push_back(arg);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
        }
    }
    
    return options;
}

void CliParser::print_help() const {
    std::cout << "Usage: " << program_name_ << " [options] <input_file> [more_files...]\n\n";
    std::cout << "Weave++ - HTML template to C++ transpiler\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help      Show this help message\n";
    std::cout << "  -v, --version   Show version information\n";
    std::cout << "  -o <path>       Output file or directory (default: current directory)\n";
    std::cout << "  --func <signature>  Function signature (default: \"void __func__(Response& resp)\")\n";
    std::cout << "  --extra <file>  Inject additional C++ code from file\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name_ << " template.html                    # Output to ./template.cpp\n";
    std::cout << "  " << program_name_ << " template.html -o myfile.cpp      # Output to myfile.cpp\n";
    std::cout << "  " << program_name_ << " template.html -o output/         # Output to output/template.cpp\n";
    std::cout << "  " << program_name_ << " *.html -o generated/             # Multiple files to directory\n";
    std::cout << "  " << program_name_ << " --func=\"void render(HttpResponse& res)\" template.html\n";
    std::cout << "  " << program_name_ << " --extra common.html template.html -o output/\n";
}

void CliParser::print_version() const {
    std::cout << "Weave++ version 1.0.0\n";
    std::cout << "HTML template to C++ transpiler\n";
}

} // namespace weave
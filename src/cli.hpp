#pragma once

#include <string>
#include <vector>

namespace weave {

struct CliOptions {
    std::vector<std::string> input_files;
    std::string output_directory = ".";
    std::string function_signature = "void __func__(HttpResponse& resp)";
    std::string extra_file = "";  // 注入文件路径
    bool show_help = false;
    bool show_version = false;
    // 向量化I/O是唯一的输出模式，不再需要选项
};

class CliParser {
public:
    CliOptions parse(int argc, char* argv[]);
    void print_help() const;
    void print_version() const;

private:
    std::string program_name_;
};

} // namespace weave
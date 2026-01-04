// list_files.cpp

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <regex>
#include <cstring>
#include <ctime>
#include <windows.h>

namespace fs = std::filesystem;

// ==================== 配置结构 ====================

struct Options {
    int depth = INT_MAX;
    bool show_size = false;
    bool show_time = false;
    bool show_type = false;
    std::string filter;
    std::string exclude;
    bool dirs_only = false;
    bool files_only = false;
    int format = 0;  // 0=tree, 1=json, 2=list
    bool relative = false;
    std::string output;
    bool compress = false;
};

struct FileInfo {
    fs::path path;
    fs::path rel_path;
    std::string name;
    bool is_dir;
    uintmax_t size;
    std::string time;
    int depth;
};

// ==================== 工具函数 ====================

// UTF-8 转宽字符（用于 Windows 中文路径）
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string format_size(uintmax_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

std::string format_time(const fs::file_time_type& ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t c_time = std::chrono::system_clock::to_time_t(sctp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&c_time));
    return buf;
}

bool wildcard_match(const std::string& text, const std::string& pattern) {
    // 简单的通配符匹配
    size_t i = 0, j = 0;
    size_t star_idx = std::string::npos, match_idx = 0;

    while (i < text.length()) {
        if (j < pattern.length() && (pattern[j] == '?' || pattern[j] == text[i])) {
            i++; j++;
        } else if (j < pattern.length() && pattern[j] == '*') {
            star_idx = j++;
            match_idx = i;
        } else if (star_idx != std::string::npos) {
            j = star_idx + 1;
            i = ++match_idx;
        } else {
            return false;
        }
    }

    while (j < pattern.length() && pattern[j] == '*') j++;

    return j == pattern.length();
}

bool matches_filter(const std::string& name, const std::string& filter_str) {
    if (filter_str.empty()) return true;

    // 检查是否是正则表达式
    if (filter_str.substr(0, 6) == "regex:") {
        try {
            std::regex pattern(filter_str.substr(6));
            return std::regex_match(name, pattern);
        } catch (...) {
            return false;
        }
    }

    // 通配符支持（逗号分隔多个模式）
    size_t start = 0, end = 0;
    while ((end = filter_str.find(',', start)) != std::string::npos) {
        std::string pattern = filter_str.substr(start, end - start);
        // 去除空格
        pattern.erase(0, pattern.find_first_not_of(" \t"));
        pattern.erase(pattern.find_last_not_of(" \t") + 1);
        if (wildcard_match(name, pattern)) return true;
        start = end + 1;
    }
    // 最后一个模式
    std::string pattern = filter_str.substr(start);
    pattern.erase(0, pattern.find_first_not_of(" \t"));
    pattern.erase(pattern.find_last_not_of(" \t") + 1);
    return wildcard_match(name, pattern);
}

bool matches_exclude(const std::string& name, const std::string& exclude_str) {
    if (exclude_str.empty()) return false;
    return matches_filter(name, exclude_str);
}

// ==================== 参数解析 ====================

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <directory> [options]\n\n"
              << "Options:\n"
              << "  -d, --depth <n>     Recursion depth (default: inf)\n"
              << "  -s, --size          Show file size\n"
              << "  -t, --time          Show modification time\n"
              << "  -T, --type          Show file type/extension\n"
              << "  -f, --filter <p>    Include filter (wildcard: *.lua or regex: regex:.*%.lua$)\n"
              << "  -e, --exclude <p>   Exclude filter\n"
              << "  --dirs-only         List directories only\n"
              << "  --files-only        List files only\n"
              << "  -F, --format <fmt>  Output format (tree/json/list, default: tree)\n"
              << "  -r, --relative      Use relative paths\n"
              << "  -o, --output <file> Output to file\n"
              << "  -c, --compress      gzip compress output (requires -o)\n"
              << "  -h, --help          Show this help\n";
}

bool parse_arguments(int argc, char** argv, Options& opt, std::string& dir) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--depth" || arg == "-d") {
            if (++i >= argc) return false;
            std::string val = argv[i];
            opt.depth = (val == "inf" || val == "INF") ? INT_MAX : std::stoi(val);
        } else if (arg == "--size" || arg == "-s") {
            opt.show_size = true;
        } else if (arg == "--time" || arg == "-t") {
            opt.show_time = true;
        } else if (arg == "--type" || arg == "-T") {
            opt.show_type = true;
        } else if (arg == "--filter" || arg == "-f") {
            if (++i >= argc) return false;
            opt.filter = argv[i];
        } else if (arg == "--exclude" || arg == "-e") {
            if (++i >= argc) return false;
            opt.exclude = argv[i];
        } else if (arg == "--dirs-only") {
            opt.dirs_only = true;
        } else if (arg == "--files-only") {
            opt.files_only = true;
        } else if (arg == "--format" || arg == "-F") {
            if (++i >= argc) return false;
            std::string fmt = argv[i];
            if (fmt == "tree") opt.format = 0;
            else if (fmt == "json") opt.format = 1;
            else if (fmt == "list") opt.format = 2;
            else return false;
        } else if (arg == "--relative" || arg == "-r") {
            opt.relative = true;
        } else if (arg == "--output" || arg == "-o") {
            if (++i >= argc) return false;
            opt.output = argv[i];
        } else if (arg == "--compress" || arg == "-c") {
            opt.compress = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option - " << arg << "\n";
            return false;
        } else {
            dir = arg;
        }
    }

    if (dir.empty()) return false;
    if (opt.dirs_only && opt.files_only) {
        std::cerr << "Error: --dirs-only and --files-only cannot be used together\n";
        return false;
    }
    if (opt.compress && opt.output.empty()) {
        std::cerr << "Error: --compress requires --output\n";
        return false;
    }

    return true;
}

// ==================== 文件遍历 ====================

std::vector<FileInfo> list_files(const std::string& dir_str, const Options& opt) {
    std::vector<FileInfo> files;
    std::wstring wdir = utf8_to_wstring(dir_str);
    fs::path root_dir(wdir);

    if (!fs::exists(root_dir)) {
        std::cerr << "Error: Path does not exist - " << dir_str << "\n";
        return files;
    }

    try {
        auto recursive = fs::recursive_directory_iterator(
            root_dir,
            fs::directory_options::skip_permission_denied
        );

        for (const auto& entry : recursive) {
            // 跳过无法访问的文件
            std::error_code ec;
            bool is_dir_val = fs::is_directory(entry.path(), ec);
            if (ec) continue;

            // 检查深度
            int depth = recursive.depth() + 1;
            if (depth > opt.depth) {
                if (is_dir_val) {
                    try {
                        recursive.disable_recursion_pending();
                    } catch (...) {}
                }
                continue;
            }

            std::string name;
            try {
                name = entry.path().filename().string();
            } catch (...) {
                continue;  // 跳过无法转换文件名的文件
            }

            // 应用过滤
            if (!matches_filter(name, opt.filter)) continue;
            if (matches_exclude(name, opt.exclude)) continue;
            if (opt.dirs_only && !is_dir_val) continue;
            if (opt.files_only && is_dir_val) continue;

            FileInfo info;
            info.path = entry.path();

            try {
                info.rel_path = opt.relative ? fs::relative(entry.path(), root_dir) : entry.path();
            } catch (...) {
                info.rel_path = entry.path();
            }

            info.name = name;
            info.is_dir = is_dir_val;

            // 获取文件大小
            if (!is_dir_val) {
                try {
                    info.size = fs::file_size(entry.path());
                } catch (...) {
                    info.size = 0;
                }
            } else {
                info.size = 0;
            }

            // 获取修改时间
            try {
                info.time = format_time(entry.last_write_time());
            } catch (...) {
                info.time = "";
            }

            info.depth = depth;

            files.push_back(info);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: " << e.what() << "\n";
    }

    return files;
}

// ==================== 输出格式 ====================

std::string output_tree(const std::vector<FileInfo>& files, const std::string& root, const Options& opt) {
    std::string result;
    result += root + "\n\n";

    for (const auto& f : files) {
        // 构建前缀（树形结构）
        std::string prefix;
        for (int i = 1; i < f.depth; i++) {
            prefix += "│   ";
        }
        if (f.depth > 0) prefix += "├── ";

        std::string type_label = f.is_dir ? "[DIR]  " : "[FILE] ";
        std::string line = prefix + type_label + (opt.relative ? f.rel_path.string() : f.name);

        // 添加额外信息
        if (opt.show_size && !f.is_dir) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%-50s%s", line.c_str(), format_size(f.size).c_str());
            line = buf;
        }
        if (opt.show_time) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%-65s%s", line.c_str(), f.time.c_str());
            line = buf;
        }
        if (opt.show_type && !f.is_dir) {
            std::string ext = f.path.extension().string();
            if (!ext.empty()) ext = ext.substr(1); // 去掉点
            line += " " + ext;
        }

        result += line + "\n";
    }

    return result;
}

// JSON 路径转义辅助函数
std::string escape_json_path(const std::string& path) {
    std::string result;
    for (char c : path) {
        if (c == '\\') {
            result += "\\\\";
        } else if (c == '"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }
    return result;
}

std::string output_json(const std::vector<FileInfo>& files, const std::string& root, const Options& opt) {
    std::string result;
    result += "{\n";
    result += "  \"root\": \"" + escape_json_path(root) + "\",\n";
    result += "  \"files\": [\n";

    for (size_t i = 0; i < files.size(); i++) {
        const auto& f = files[i];
        std::string path_str = opt.relative ? f.rel_path.string() : f.path.string();
        result += "    {";
        result += "\"path\": \"" + escape_json_path(path_str) + "\", ";
        result += "\"type\": " + std::string(f.is_dir ? "\"dir\"" : "\"file\"") + ", ";
        result += "\"name\": \"" + f.name + "\"";

        if (opt.show_size && !f.is_dir) {
            result += ", \"size\": " + std::to_string(f.size);
        }
        if (opt.show_time) {
            result += ", \"modified\": \"" + f.time + "\"";
        }
        if (opt.show_type && !f.is_dir) {
            std::string ext = f.path.extension().string();
            if (!ext.empty()) ext = ext.substr(1);
            result += ", \"ext\": \"" + ext + "\"";
        }

        result += "}" + std::string(i < files.size() - 1 ? "," : "") + "\n";
    }

    result += "  ]\n}\n";
    return result;
}

std::string output_list(const std::vector<FileInfo>& files, const Options& opt) {
    std::string result;
    for (const auto& f : files) {
        result += (opt.relative ? f.rel_path.string() : f.path.string()) + "\n";
    }
    return result;
}

void write_output(const std::string& content, const Options& opt) {
    if (opt.output.empty()) {
        std::cout << content;
    } else {
        std::string out_file = opt.output;
        if (opt.compress) {
            out_file += ".gz";
            // TODO: Implement gzip compression
            std::cerr << "Warning: gzip compression not implemented, outputting uncompressed file\n";
        }

        std::ofstream out(out_file, std::ios::binary);
        if (!out) {
            std::cerr << "Error: Cannot create output file - " << out_file << "\n";
            return;
        }
        out << content;
        out.close();

        std::cout << "Output saved to: " << out_file << "\n";
    }
}

// ==================== 主函数 ====================

int main(int argc, char** argv) {
    // 设置控制台为 UTF-8 模式
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Options opt;
    std::string dir;

    if (!parse_arguments(argc, argv, opt, dir)) {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<FileInfo> files = list_files(dir, opt);

    std::string output;
    switch (opt.format) {
        case 0: output = output_tree(files, dir, opt); break;
        case 1: output = output_json(files, dir, opt); break;
        case 2: output = output_list(files, opt); break;
    }

    write_output(output, opt);

    return 0;
}

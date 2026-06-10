#pragma once
#include <filesystem>
#include <string>
#include <fstream>
#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

class FileSystemManager {
private:
    std::string root_dir;

    json scan_directory(const fs::path& dir_path) {
        json node = json::array();
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            json item;
            // Get the raw filename (e.g., "main.cpp")
            item["name"] = entry.path().filename().string();
            // Get the relative path for the UI (e.g., "src/main.cpp")
            item["path"] = fs::relative(entry.path(), root_dir).string();

            if (entry.is_directory()) {
                item["type"] = "folder";
                item["children"] = scan_directory(entry.path());
            } else {
                item["type"] = "file";
            }
            node.push_back(item);
        }
        return node;
    }

public:
    FileSystemManager(const std::string& root) : root_dir(root) {
        // Automatically create the workspace folder if it doesn't exist
        if (!fs::exists(root_dir)) {
            fs::create_directories(root_dir);
        }
    }

    json get_file_tree() {
        return scan_directory(root_dir);
    }

    bool create_file(const std::string& rel_path) {
        fs::path full_path = fs::path(root_dir) / rel_path;
        
        // Automatically create parent directories if they typed "src/app.cpp"
        if (full_path.has_parent_path()) {
            fs::create_directories(full_path.parent_path());
        }

        if (fs::exists(full_path)) return false; // Don't overwrite existing files
        
        // Create the file and immediately close the lock on it
        std::ofstream out(full_path);
        bool success = out.good();
        out.close(); 
        
        return success;
    }
};
#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <shared_mutex>

class StateMirror {
private:
    std::unordered_map<std::string, std::vector<std::string>> local_files;
    mutable std::shared_mutex rw_mtx;

public:
    void apply_cloud_delta(const std::string& path, const std::string& action, int row, int col, const std::string& text) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);
        // Apply the incoming diff to keep the local buffer aligned with the cloud
    }

    void overwrite_file(const std::string& path, const std::vector<std::string>& full_text) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);
        local_files[path] = full_text;
    }

    std::string get_file_content(const std::string& path) const {
        std::shared_lock<std::shared_mutex> lock(rw_mtx);
        std::string result;
        auto it = local_files.find(path);
        if (it != local_files.end()) {
            for (const auto& line : it->second) {
                result += line + "\n";
            }
        }
        return result;
    }
};
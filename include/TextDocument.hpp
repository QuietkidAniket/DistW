#pragma once
#include <vector>
#include <string>
#include <shared_mutex>

class TextDocument {
private:
    std::vector<std::string> lines;
    mutable std::shared_mutex rw_mtx;

public:
    TextDocument() { 
        lines.push_back(""); // Initialize with 1 empty line
    }

    void insert_text(int row, int col, const std::string& text) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);
        if (row < lines.size() && col <= lines[row].length()) {
            lines[row].insert(col, text);
        }
    }

    void delete_text(int row, int col, int length) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);
        if (row < lines.size() && col < lines[row].length()) {
            // Prevent erasing past the end of the line
            int erase_len = std::min(length, static_cast<int>(lines[row].length() - col));
            lines[row].erase(col, erase_len);
        }
    }

    void insert_newline(int row, int col) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx);
        if (row < lines.size() && col <= lines[row].length()) {
            // Split the line at the cursor
            std::string remainder = lines[row].substr(col);
            lines[row] = lines[row].substr(0, col);
            // Insert the remainder as a new line right below
            lines.insert(lines.begin() + row + 1, remainder);
        }
    }
    void set_full_text(const std::string& text) {
        lines.clear();
        std::stringstream ss(text);
        std::string line;
        
        while (std::getline(ss, line)) {
            // Strip hidden Windows carriage returns if present
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        
        // Ensure the document always has at least one valid line
        if (lines.empty()) {
            lines.push_back(""); 
        }
    }

    // Used for State Reconciliation (Full Sync) when a user reconnects
    std::string get_full_text() const {
        std::shared_lock<std::shared_mutex> lock(rw_mtx);
        std::string result;
        for (const auto& line : lines) {
            result += line + "\n";
        }
        return result;
    }
};
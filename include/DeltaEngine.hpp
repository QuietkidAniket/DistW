#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <shared_mutex>

#include "TextDocument.hpp"
#include "HierarchicalLockTree.hpp"

class DeltaEngine {
private:
    HierarchicalLockTree& hdlm; 
    std::unordered_map<std::string, std::shared_ptr<TextDocument>> mp_docs;
    mutable std::shared_mutex map_mtx;

    std::shared_ptr<TextDocument> get_or_create_doc(const std::string& path) {
        std::unique_lock<std::shared_mutex> lock(map_mtx);
        if (mp_docs.find(path) == mp_docs.end()) {
            mp_docs[path] = std::make_shared<TextDocument>();
        }
        return mp_docs[path];
    }

public:
    DeltaEngine(HierarchicalLockTree& lock_tree) : hdlm(lock_tree) {}

    bool process_delta(const std::string& user_id, const std::string& action, 
                       const std::string& path, int row, int col, 
                       const std::string& text, int length) 
    {
        if (!hdlm.is_exclusive_owner(path, user_id)) return false; 
        auto doc = get_or_create_doc(path);
        if (action == "INSERT") doc->insert_text(row, col, text);
        else if (action == "DELETE") doc->delete_text(row, col, length);
        else if (action == "NEWLINE") doc->insert_newline(row, col);
        return true; 
    }

    // 🚨 NEW METHOD: Directly overwrites the file state from the Master UI
    bool force_state(const std::string& user_id, const std::string& path, const std::string& full_text) {
        // STEP 1: Security Gate (Ensure they actually hold the write lock!)
        if (!hdlm.is_exclusive_owner(path, user_id)) {
            return false; 
        }

        // STEP 2: Overwrite the document
        auto doc = get_or_create_doc(path);
        doc->set_full_text(full_text); 
        return true;
    }

    std::string generate_full_sync(const std::string& path) {
        return get_or_create_doc(path)->get_full_text();
    }
};
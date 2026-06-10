#pragma once
#include "LockNode.hpp"
#include <iostream>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <string>

/**
 * @file HierarchicalLockTree.hpp
 * @brief 
 *      The intent lock trie focusses on locking and unlocking the subdirectories and files
 *      without causing race conditions while traversing the tree
*/

class HierarchicalLockTree {
private:
    std::shared_ptr<LockNode> root;
    
    // Global lock specifically for the multi-resource Fail-Fast batch requests
    std::mutex global_tree_mutex; 

    // Internal helper to split "/src/main.cpp" into ["src", "main.cpp"]
    std::vector<std::string> tokenize_path(const std::string& path);

    // Internal dry-run verifier for atomic batch locking
    bool verify_path_availability(const std::vector<std::string>& tokens, LockMode mode);

public:
    HierarchicalLockTree() {
        root = std::make_shared<LockNode>("ROOT");
    }

    //-------------------------------------------------------------------------
    // Core Acquisition & Release API
    //-------------------------------------------------------------------------
    
    // Standard single-file request (with Admin preemption support)
    std::string acquire_single_lock(const std::string& path, const std::string& user_id, LockMode mode, bool is_admin);

    // Multi-file atomic request (Fail-Fast: all succeed or all fail)
    std::string acquire_locks_atomically(const std::vector<std::string>& paths, const std::string& user_id, LockMode mode);

    // Unwinds the intent locks and auto-promotes queued users
    void release_lock(const std::string& path, const std::string& user_id);

    //-------------------------------------------------------------------------
    // Utility & Verification API (Used by other Modules)
    //-------------------------------------------------------------------------

    // Used by DeltaEngine to strictly verify text-diff ownership
    bool is_exclusive_owner(const std::string& path, const std::string& user_id) {
        std::vector<std::string> tokens = tokenize_path(path);
        std::shared_ptr<LockNode> current = root;
        
        for (const std::string& segment : tokens) {
            std::shared_lock<std::shared_mutex> lock(current->rw_mtx);
            auto it = current->mp_children.find(segment);
            if (it == current->mp_children.end()) return false;
            current = it->second;
        }

        std::shared_lock<std::shared_mutex> lock(current->rw_mtx);
        return (current->current_mode == LockMode::EXCLUSIVE && current->exclusive_owner == user_id);
    }

    // Used by WebSocketRouter to broadcast lock state to the React UI
    std::unordered_map<std::string, std::string> get_all_active_locks();
};

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <shared_mutex>
#include <atomic>

enum class LockMode {
    UNLOCKED,
    INTENT,      // A child of this node has an active lock
    SHARED,      // Read-only lock (multiple users allowed)
    EXCLUSIVE    // Write lock (single user only)
};

struct LockRequest {
    std::string user_id;
    LockMode requested_mode;
};

class LockNode {
public:
    std::string path_segment; 
    LockMode current_mode = LockMode::UNLOCKED;
    
    // Ownership tracking
    std::string exclusive_owner = ""; 
    std::unordered_set<std::string> shared_owners;

    // Starvation Prevention Mechanism (Write-Preferring Queue)
    int pending_exclusive_requests = 0; 
    std::queue<LockRequest> wait_queue;

    // O(1) Release Ascent Tracker
    // Tracks how many active locks exist in the sub-tree beneath this node.
    std::atomic<int> active_locks_beneath{0};

    // Trie Traversal Map
    std::unordered_map<std::string, std::shared_ptr<LockNode>> mp_children;
    
    // Fine-grained lock protecting THIS specific node's state
    mutable std::shared_mutex rw_mtx; 

    explicit LockNode(std::string segment) : path_segment(std::move(segment)) {}

    // Helper to check if a new shared lock can be granted.
    // Enforces the Write-Preferring policy to prevent writer starvation.
    bool can_grant_shared() const {
        if (pending_exclusive_requests > 0) {
            return false;
        }
        return current_mode == LockMode::UNLOCKED || 
               current_mode == LockMode::SHARED || 
               current_mode == LockMode::INTENT;
    }
};
#include "HierarchicalLockTree.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>

// Helper to tokenize "/src/network/main.cpp" into ["src", "network", "main.cpp"]
std::vector<std::string> HierarchicalLockTree::tokenize_path(const std::string& path) {
    std::vector<std::string> res;
    std::stringstream ss(path);
    std::string tmp;
    while (std::getline(ss, tmp, '/')) {
        if (!tmp.empty()) res.push_back(tmp);
    }
    return res;
}

// -------------------------------------------------------------------------
// ACQUIRE SINGLE LOCK
// -------------------------------------------------------------------------
std::string HierarchicalLockTree::acquire_single_lock(
    const std::string& path, const std::string& user_id, LockMode requested_mode, bool is_admin) 
{
    std::vector<std::string> tokens = tokenize_path(path);
    if (tokens.empty()) return "DENIED: Invalid Path";

    std::shared_ptr<LockNode> current = root;
    std::vector<std::shared_ptr<LockNode>> traversal_path; 
    traversal_path.push_back(current);

    // Phase 1: The Descent (Intent Verification)
    for (size_t i = 0; i < tokens.size() - 1; i++) {
        const std::string& segment = tokens[i];
        std::unique_lock<std::shared_mutex> parent_lock(current->rw_mtx);

        if (current->current_mode == LockMode::EXCLUSIVE && current->exclusive_owner != user_id) {
            if (!is_admin) {
                if (requested_mode == LockMode::EXCLUSIVE) current->pending_exclusive_requests++;
                current->wait_queue.push({user_id, requested_mode});
                return "QUEUED: Parent directory locked";
            }
        }

        if (current->mp_children.find(segment) == current->mp_children.end()) {
            current->mp_children[segment] = std::make_shared<LockNode>(segment);
        }

        current = current->mp_children[segment];
        traversal_path.push_back(current);
    }

    // Phase 2: The Acquisition (Target Node)
    const std::string& target_segment = tokens.back();
    std::unique_lock<std::shared_mutex> final_parent_lock(current->rw_mtx);
    if (current->mp_children.find(target_segment) == current->mp_children.end()) {
        current->mp_children[target_segment] = std::make_shared<LockNode>(target_segment);
    }
    std::shared_ptr<LockNode> target_node = current->mp_children[target_segment];
    final_parent_lock.unlock(); 

    std::unique_lock<std::shared_mutex> target_lock(target_node->rw_mtx);

    // ADMIN OVERRIDE
    if (is_admin) {
        target_node->current_mode = requested_mode;
        if (requested_mode == LockMode::EXCLUSIVE) {
            target_node->exclusive_owner = user_id;
            target_node->shared_owners.clear();
        } else {
            target_node->shared_owners.insert(user_id);
            target_node->exclusive_owner = "";
        }
        return "ACQUIRED_BY_PREEMPTION";
    }

    // STANDARD ACQUISITION
    if (requested_mode == LockMode::EXCLUSIVE) {
        if (target_node->current_mode != LockMode::UNLOCKED && target_node->exclusive_owner != user_id) {
            target_node->pending_exclusive_requests++;
            target_node->wait_queue.push({user_id, requested_mode});
            return "QUEUED: Conflict at target";
        }
        target_node->current_mode = LockMode::EXCLUSIVE;
        target_node->exclusive_owner = user_id;
    } 
    else if (requested_mode == LockMode::SHARED) {
        if (!target_node->can_grant_shared()) {
            target_node->wait_queue.push({user_id, requested_mode});
            return "QUEUED: Writers have priority";
        }
        target_node->current_mode = LockMode::SHARED;
        target_node->shared_owners.insert(user_id);
    }

    // Phase 3: The Ascent (Setting Intent)
    for (auto& node : traversal_path) {
        std::unique_lock<std::shared_mutex> lock(node->rw_mtx);
        if (node->current_mode == LockMode::UNLOCKED) {
            node->current_mode = LockMode::INTENT;
        }
    }

    return "ACQUIRED";
}

// -------------------------------------------------------------------------
// ACQUIRE MULTIPLE LOCKS ATOMICALLY (FAIL-FAST)
// -------------------------------------------------------------------------
std::string HierarchicalLockTree::acquire_locks_atomically(
    const std::vector<std::string>& paths, const std::string& user_id, LockMode mode) 
{
    if (paths.empty()) return "DENIED: Empty request";

    std::vector<std::string> sorted_paths = paths;
    std::sort(sorted_paths.begin(), sorted_paths.end());

    std::unique_lock<std::mutex> global_lock(global_tree_mutex);

    for (const std::string& path : sorted_paths) {
        std::vector<std::string> tokens = tokenize_path(path);
        if (!verify_path_availability(tokens, mode)) {
            return "DENIED_BATCH: Conflict detected on " + path;
        }
    }

    for (const std::string& path : sorted_paths) {
        acquire_single_lock(path, user_id, mode, false); 
    }

    return "ACQUIRED_BATCH";
}

// -------------------------------------------------------------------------
// VERIFY PATH AVAILABILITY (DRY-RUN)
// -------------------------------------------------------------------------
bool HierarchicalLockTree::verify_path_availability(const std::vector<std::string>& tokens, LockMode mode) {
    if (tokens.empty()) return false;
    std::shared_ptr<LockNode> current = root;

    for (size_t i = 0; i < tokens.size() - 1; i++) {
        const std::string& segment = tokens[i];
        std::shared_lock<std::shared_mutex> lock(current->rw_mtx);

        if (current->current_mode == LockMode::EXCLUSIVE) return false;

        auto it = current->mp_children.find(segment);
        if (it == current->mp_children.end()) return true;
        current = it->second;
    }

    const std::string& target_segment = tokens.back();
    std::shared_lock<std::shared_mutex> lock(current->rw_mtx);
    
    auto it = current->mp_children.find(target_segment);
    if (it == current->mp_children.end()) return true;

    std::shared_ptr<LockNode> target_node = it->second;
    std::shared_lock<std::shared_mutex> target_lock(target_node->rw_mtx);

    if (mode == LockMode::EXCLUSIVE) {
        return target_node->current_mode == LockMode::UNLOCKED;
    } else {
        return target_node->can_grant_shared();
    }
}

// -------------------------------------------------------------------------
// RELEASE LOCK & UNWIND
// -------------------------------------------------------------------------
void HierarchicalLockTree::release_lock(const std::string& path, const std::string& user_id) {
    std::vector<std::string> tokens = tokenize_path(path);
    if (tokens.empty()) return;

    std::shared_ptr<LockNode> current = root;
    std::vector<std::shared_ptr<LockNode>> traversal_path;
    traversal_path.push_back(current);

    for (const std::string& segment : tokens) {
        std::shared_lock<std::shared_mutex> read_lock(current->rw_mtx);
        auto it = current->mp_children.find(segment);
        if (it == current->mp_children.end()) return; 
        current = it->second;
        traversal_path.push_back(current);
    }

    std::shared_ptr<LockNode> target_node = current;
    std::unique_lock<std::shared_mutex> target_lock(target_node->rw_mtx);

    bool was_actually_locked = false;
    if (target_node->current_mode == LockMode::EXCLUSIVE && target_node->exclusive_owner == user_id) {
        target_node->exclusive_owner = "";
        target_node->current_mode = LockMode::UNLOCKED;
        was_actually_locked = true;
    } 
    else if (target_node->current_mode == LockMode::SHARED) {
        auto it = target_node->shared_owners.find(user_id);
        if (it != target_node->shared_owners.end()) {
            target_node->shared_owners.erase(it);
            if (target_node->shared_owners.empty()) target_node->current_mode = LockMode::UNLOCKED;
            was_actually_locked = true;
        }
    }

    if (!was_actually_locked) return;

    if (!target_node->wait_queue.empty() && target_node->current_mode == LockMode::UNLOCKED) {
        LockRequest next_req = target_node->wait_queue.front();
        target_node->wait_queue.pop();

        target_node->current_mode = next_req.requested_mode;
        if (next_req.requested_mode == LockMode::EXCLUSIVE) {
            target_node->exclusive_owner = next_req.user_id;
            target_node->pending_exclusive_requests--; 
        } else {
            target_node->shared_owners.insert(next_req.user_id);
        }
        target_lock.unlock(); 
        return; 
    }
    target_lock.unlock();

    for (int i = static_cast<int>(traversal_path.size()) - 2; i >= 0; i--) {
        std::shared_ptr<LockNode> parent = traversal_path[i];
        int remaining_locks = --(parent->active_locks_beneath);
        if (remaining_locks == 0) {
            std::unique_lock<std::shared_mutex> parent_lock(parent->rw_mtx);
            if (parent->active_locks_beneath.load() == 0 && parent->current_mode == LockMode::INTENT) {
                parent->current_mode = LockMode::UNLOCKED;
            }
        }
    }
}

// -------------------------------------------------------------------------
// GET ALL ACTIVE LOCKS FOR UI
// -------------------------------------------------------------------------
std::unordered_map<std::string, std::string> HierarchicalLockTree::get_all_active_locks() {
    std::unordered_map<std::string, std::string> active_locks;

    std::function<void(const std::shared_ptr<LockNode>&, const std::string&)> dfs = 
        [&](const std::shared_ptr<LockNode>& node, const std::string& current_path) 
    {
        std::shared_lock<std::shared_mutex> lock(node->rw_mtx);

        if (node->current_mode == LockMode::EXCLUSIVE && !node->exclusive_owner.empty()) {
            active_locks[current_path] = node->exclusive_owner;
        } 
        else if (node->current_mode == LockMode::SHARED && !node->shared_owners.empty()) {
            std::string owners = "";
            for (const auto& owner : node->shared_owners) {
                owners += owner + ",";
            }
            owners.pop_back(); 
            active_locks[current_path] = owners;
        }

        for (const auto& [segment, child_node] : node->mp_children) {
            std::string next_path = current_path.empty() ? segment : current_path + "/" + segment;
            dfs(child_node, next_path);
        }
    };

    std::shared_lock<std::shared_mutex> root_lock(root->rw_mtx);
    for (const auto& [segment, child_node] : root->mp_children) {
        dfs(child_node, segment);
    }

    return active_locks;
}
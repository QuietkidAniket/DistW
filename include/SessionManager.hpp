#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <iostream>

#include "HierarchicalLockTree.hpp"

struct Session {
    std::string session_id;
    std::atomic<int> cnt_users{0};
    
    // We use atomic so we can update the timestamp without locking the whole session
    std::atomic<std::chrono::steady_clock::time_point> last_active;
    
    std::unordered_map<std::string, std::vector<std::string>> mp_files;
    std::unique_ptr<HierarchicalLockTree> lock_tree;
    mutable std::shared_mutex rw_mtx;

    Session() {
        last_active = std::chrono::steady_clock::now();
        lock_tree = std::make_unique<HierarchicalLockTree>();
    }

    void ping() {
        last_active = std::chrono::steady_clock::now();
    }
};

class SessionManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Session>> mp_sessions;
    mutable std::shared_mutex global_mtx;

public:
    std::shared_ptr<Session> get_session(const std::string& id) {
        std::shared_lock<std::shared_mutex> lock(global_mtx);
        auto it = mp_sessions.find(id);
        if (it != mp_sessions.end()) {
            it->second->ping(); // Update activity timestamp on access
            return it->second;
        }
        return nullptr;
    }

    std::string create_session(const std::string& id) {
        std::unique_lock<std::shared_mutex> lock(global_mtx);
        if (mp_sessions.find(id) == mp_sessions.end()) {
            mp_sessions[id] = std::make_shared<Session>();
            mp_sessions[id]->session_id = id;
        }
        return id;
    }

    // Called by the Reaper Engine
    void cull_stale_sessions(int timeout_minutes) {
        std::unique_lock<std::shared_mutex> lock(global_mtx);
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = mp_sessions.begin(); it != mp_sessions.end(); ) {
            auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - it->second->last_active.load());
            
            if (duration.count() > timeout_minutes) {
                std::cout << "[Reaper] Terminating stale session: " << it->first << "\n";
                // std::shared_ptr automatically cleans up the Lock Tree and Text Buffers
                it = mp_sessions.erase(it); 
            } else {
                ++it;
            }
        }
    }
};
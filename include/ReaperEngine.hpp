#pragma once
#include <thread>
#include <chrono>
#include <iostream>
#include "SessionManager.hpp"

class ReaperEngine {
private:
    SessionManager& session_manager;
    std::jthread reaper_thread;
    
    // Hard limit: 20 minutes of inactivity before RAM is reclaimed
    const int TIMEOUT_MINUTES = 20;

    void sweep_loop(std::stop_token stoken) {
        std::cout << "[Reaper] Garbage collection engine initialized.\n";
        
        // Loop runs until the server program is forcefully stopped
        while (!stoken.stop_requested()) {
            // Sleep for 60 seconds, but wake up immediately if a stop is requested
            std::unique_lock<std::mutex> lock(sleep_mtx);
            cv.wait_for(lock, std::chrono::seconds(60), [&stoken] { 
                return stoken.stop_requested(); 
            });

            if (stoken.stop_requested()) {
                break;
            }

            // Trigger the memory cleanup
            session_manager.cull_stale_sessions(TIMEOUT_MINUTES);
        }
        
        std::cout << "[Reaper] Engine shutting down cleanly.\n";
    }

    std::mutex sleep_mtx;
    std::condition_variable cv;

public:
    ReaperEngine(SessionManager& sm) : session_manager(sm) {
        // Automatically starts the background execution
        reaper_thread = std::jthread([this](std::stop_token stoken) {
            this->sweep_loop(stoken);
        });
    }

    // Explicit destructor is not strictly needed for std::jthread, 
    // but useful if we want to explicitly trigger the stop.
    ~ReaperEngine() {
        reaper_thread.request_stop();
        cv.notify_all(); // Wake the thread up if it's currently sleeping
    }
};
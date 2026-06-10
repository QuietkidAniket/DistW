#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <App.h>
#include <json.hpp>

#include "SessionManager.hpp"
#include "HierarchicalLockTree.hpp"
#include "DeltaEngine.hpp"
#include "AdminWarmPool.hpp"
#include "FileSystemManager.hpp" 

using json = nlohmann::json;

struct UserSocketData {
    std::string user_id;
    std::string session_id;
    bool is_admin = false;
};

/**
 * @brief This code has been compiled from various sources 
 * while being assisted by AI
 */
class WebSocketRouter {
private:
    SessionManager& session_manager;
    HierarchicalLockTree& hdlm;
    DeltaEngine& delta_engine;
    AdminWarmPool& admin_pool;
    FileSystemManager& fs_manager; 

    // Helper to tokenize the URL path
    std::vector<std::string> extract_url_tokens(const std::string& url) {
        std::vector<std::string> tokens;
        std::stringstream ss(url);
        std::string item;
        while (std::getline(ss, item, '/')) {
            if (!item.empty()) tokens.push_back(item);
        }
        return tokens;
    }

public:
    WebSocketRouter(SessionManager& sm, HierarchicalLockTree& lock_tree, 
                    DeltaEngine& delta, AdminWarmPool& pool, FileSystemManager& fsm)
        : session_manager(sm), hdlm(lock_tree), delta_engine(delta), admin_pool(pool), fs_manager(fsm) {}

    void start_server(int port) {
        uWS::App().ws<UserSocketData>("/*", {
            
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 960,
            .maxBackpressure = 1 * 1024 * 1024,
            
            //--------------------------------------------------------------
            // REAL HANDSHAKE & PARSING LOGIC
            //--------------------------------------------------------------
            .upgrade = [this](auto *res, auto *req, auto *context) {
                std::string url(req->getUrl()); 
                std::vector<std::string> tokens = extract_url_tokens(url);

                std::string session_id = "default";
                std::string user_id = "anonymous";

                // Expecting URL format: /session/{session_id}/user/{user_id}
                if (tokens.size() >= 4 && tokens[0] == "session" && tokens[2] == "user") {
                    session_id = tokens[1];
                    user_id = tokens[3];
                }

                // Security Check: Look for an Authorization header
                // @TEST Hardcoded to true for development testing
                bool is_admin = true; 

                // Commit the parsed data into the socket's memory block
                res->template upgrade<UserSocketData>(
                    {user_id, session_id, is_admin},
                    req->getHeader("sec-websocket-key"),
                    req->getHeader("sec-websocket-protocol"),
                    req->getHeader("sec-websocket-extensions"),
                    context
                );
            },

            .open = [this](auto *ws) {
                UserSocketData* data = ws->getUserData();
                ws->subscribe(data->session_id);
                session_manager.create_session(data->session_id);
                std::cout << "User " << data->user_id << " connected to " << data->session_id 
                          << " (Admin: " << (data->is_admin ? "Yes" : "No") << ")\n";
            },

            .message = [this](auto *ws, std::string_view message, uWS::OpCode opCode) {
                this->handle_message(ws, message);
            },

            .close = [this](auto *ws, int code, std::string_view message) {
                UserSocketData* data = ws->getUserData();
                // hdlm.release_all_locks_for_user(data->user_id);
                std::cout << "User " << data->user_id << " disconnected.\n";
            }

        }).listen(port, [port](auto *listen_socket) {
            if (listen_socket) {
                std::cout << "DistW Router running on port " << port << "...\n";
            } else {
                std::cerr << "Failed to bind to port " << port << "\n";
            }
        }).run(); 
    }

private:
    //--------------------------------------------------------------
    // THE MESSAGE ROUTING PIPELINE
    //--------------------------------------------------------------
    void handle_message(auto *ws, std::string_view raw_message) {
        UserSocketData* data = ws->getUserData();
        
        try {
            json payload = json::parse(raw_message);
            std::string type = payload.value("type", "");

            // ROUTE 1: Text Synchronization (UPDATED FOR MONACO ARRAYS)
            // ROUTE 1: Text Synchronization (UPDATED FOR MONACO ARRAYS & MEMORY)
            if (type == "DELTA") {
                std::string full_text = payload.value("full_text", "");
                std::string target_file = payload.value("file_path", "");

                // 1. Save to memory AND check the HDLM lock simultaneously
                if (delta_engine.force_state(data->user_id, target_file, full_text)) {
                    // 2. If valid, broadcast visual changes to everyone else
                    std::string serialized = payload.dump();
                    ws->publish(data->session_id, serialized, uWS::OpCode::TEXT);
                } else {
                    // 3. Security failure / Desync! Force the UI to revert.
                    json revert = {
                        {"type", "STATE_REVERT"},
                        {"file_path", target_file},
                        {"full_text", delta_engine.generate_full_sync(target_file)}
                    };
                    ws->send(revert.dump(), uWS::OpCode::TEXT);
                }
            }
            
            // ROUTE 2: Lock Management
            else if (type == "LOCK_REQUEST") {
                LockMode mode = (payload.value("mode", "") == "EXCLUSIVE") ? LockMode::EXCLUSIVE : LockMode::SHARED;
                
                std::string result = hdlm.acquire_single_lock(
                    payload.value("file_path", ""), 
                    data->user_id, 
                    mode, 
                    data->is_admin
                );

                json response = {
                    {"type", "LOCK_RESPONSE"},
                    {"file_path", payload["file_path"]},
                    {"status", result} 
                };
                ws->send(response.dump(), uWS::OpCode::TEXT);

                if (result == "ACQUIRED" || result == "ACQUIRED_BY_PREEMPTION") {
                    // Fetch real locks from HDLM and let nlohmann::json auto-format it!
                    json broadcast_payload = {
                        {"type", "LOCK_SYNC"},
                        {"active_locks", hdlm.get_all_active_locks()} 
                    };
                    
                    std::string sync_msg = broadcast_payload.dump();
                    
                    ws->publish(data->session_id, sync_msg, uWS::OpCode::TEXT);
                    ws->send(sync_msg, uWS::OpCode::TEXT); 
                }
            }
            // ROUTE 2.5: Lock Release
            else if (type == "LOCK_RELEASE") {
                hdlm.release_lock(payload.value("file_path", ""), data->user_id);
                
                // Broadcast the updated lock state to everyone
                json broadcast_payload = {
                    {"type", "LOCK_SYNC"},
                    {"active_locks", hdlm.get_all_active_locks()}
                };
                std::string sync_msg = broadcast_payload.dump();
                ws->publish(data->session_id, sync_msg, uWS::OpCode::TEXT);
                ws->send(sync_msg, uWS::OpCode::TEXT);
            }

            // ROUTE 3: Admin Preemption & Execution Sandbox
            else if (type == "ADMIN_RUN") { 
                std::string cpp_code = payload.value("code", ""); 
                std::cout << "🚀 Execution started..." << std::endl;

                std::string output = admin_pool.execute_code(cpp_code);
                
                try {
                    json response = {
                        {"type", "RUN_OUTPUT"},
                        {"data", output}
                    };
                    
                    std::string serialized = response.dump();
                    std::cout << "📤 Sending " << serialized.length() << " bytes to UI." << std::endl;
                    ws->send(serialized, uWS::OpCode::TEXT);
                } catch (const std::exception& e) {
                    std::cerr << "❌ JSON Serialization failed: " << e.what() << std::endl;
                    ws->send("{\"type\":\"RUN_OUTPUT\",\"data\":\"[Server Error: Output contained invalid characters]\"}", uWS::OpCode::TEXT);
                }
            }

            // ROUTE 4: File Content Request
            else if (type == "FILE_REQUEST") {
                std::string target_file = payload.value("file_path", "");
                
                std::string content = delta_engine.generate_full_sync(target_file);
                
                json response = {
                    {"type", "FILE_CONTENT"},
                    {"file_path", target_file},
                    {"content", content}
                };
                
                ws->send(response.dump(), uWS::OpCode::TEXT);
            }
            // ROUTE 6: Create New File
            else if (type == "FILE_CREATE") {
                std::string new_file = payload.value("file_path", "");
                std::cout << "📁 Request received to create: " << new_file << std::endl;
                
                if (fs_manager.create_file(new_file)) {
                    std::cout << "✅ File created on disk! Broadcasting new tree..." << std::endl;
                    json broadcast = {
                        {"type", "TREE_SYNC"},
                        {"tree", fs_manager.get_file_tree()}
                    };
                    std::string msg = broadcast.dump();
                    ws->publish(data->session_id, msg, uWS::OpCode::TEXT);
                    ws->send(msg, uWS::OpCode::TEXT);
                } else {
                    std::cout << "❌ Failed to create file! (Already exists or bad permissions)" << std::endl;
                }
            }
        } catch (const json::exception& e) {
            std::cerr << "Malformed JSON packet dropped: " << e.what() << "\n";
        }
    }
};
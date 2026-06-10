#include <iostream>
#include <string>
#include <json.hpp>

#include "LocalGateway.hpp"
#include "StateMirror.hpp"
#include "LocalExecutionEngine.hpp"

using json = nlohmann::json;

int main() {
    std::cout << "========================================\n";
    std::cout << "  DistW Edge Worker Daemon Initializing \n";
    std::cout << "========================================\n";

    // 1. Initialize the Core Components
    LocalGateway gateway;
    StateMirror mirror;
    LocalExecutionEngine docker_engine;

    // 2. Define the Cloud Uplink Callback (Receiving from Cloud Server)
    auto on_cloud_message = [&](const std::string& raw_message) {
        try {
            json payload = json::parse(raw_message);
            std::string type = payload.value("type", "");

            if (type == "DELTA") {
                // Keep the local text buffer perfectly aligned with the cloud
                mirror.apply_cloud_delta(
                    payload["file_path"],
                    payload["action"],
                    payload["row"],
                    payload["col"],
                    payload["text"]
                );
                // Forward the diff to the local React UI so Monaco updates
                gateway.send_to_ui(raw_message);
            } 
            else if (type == "STATE_REVERT") {
                // Cloud rejected our local changes (e.g., lock violation)
                // Force overwrite the local file state
                std::vector<std::string> full_text; // Parse from payload in reality
                mirror.overwrite_file(payload["file_path"], full_text);
                gateway.send_to_ui(raw_message); 
            }
            else if (type == "CONFIG_UPDATE") {
                // Admin changed the environment. Rebuild local Docker image.
                docker_engine.rebuild_environment(payload["dockerfile"]);
            }
            else {
                // Pass any other payloads (like LOCK_SYNC) directly to the UI
                gateway.send_to_ui(raw_message);
            }
        } catch (const json::exception& e) {
            std::cerr << "[Edge] Cloud payload parsing error: " << e.what() << "\n";
        }
    };

    // 3. Define the Local UI Callback (Receiving from User's Browser)
    auto on_ui_message = [&](const std::string& raw_message) {
        try {
            json payload = json::parse(raw_message);
            std::string type = payload.value("type", "");

            if (type == "LOCAL_RUN") {
                std::string target_file = payload.value("file_path", "main.cpp");
                
                // 1. Fetch the exact code state from our local mirror
                std::string current_code = mirror.get_file_content(target_file);
                
                // 2. Execute natively via Docker
                std::cout << "[Edge] Compiling and Executing locally...\n";
                std::string output = docker_engine.execute_code(target_file, current_code);

                // 3. Route the stdout exclusively back to the local XTerm.js terminal
                json response = {
                    {"type", "RUN_OUTPUT"},
                    {"data", output}
                };
                gateway.send_to_ui(response.dump());
            } 
            else {
                // If it's a DELTA or LOCK_REQUEST, blindly proxy it up to the Cloud Server
                gateway.send_to_cloud(raw_message);
            }
        } catch (const json::exception& e) {
            std::cerr << "[Edge] UI payload parsing error: " << e.what() << "\n";
        }
    };

    // 4. Wire the Callbacks to the Gateway
    gateway.set_callbacks(on_cloud_message, on_ui_message);

    // 5. Ignite the Network Interfaces (Blocking calls in a real implementation)
    // You would run these on separate std::jthreads so they don't block each other
    gateway.connect_to_cloud("ws://your-cloud-server-ip:9001");
    gateway.start_local_server(3000); 

    return 0;
}
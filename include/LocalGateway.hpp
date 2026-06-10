#pragma once
#include <string>
#include <functional>
#include <iostream>

class LocalGateway {
private:
    std::string session_token;
    std::function<void(const std::string&)> on_cloud_message;
    std::function<void(const std::string&)> on_ui_message;

public:
    void set_callbacks(auto cloud_cb, auto ui_cb) {
        on_cloud_message = cloud_cb;
        on_ui_message = ui_cb;
    }

    // Connects to the central cloud server (e.g., via Boost.Beast or uWS client)
    void connect_to_cloud(const std::string& wss_url) {
        std::cout << "[LocalGateway] Connecting to cloud at " << wss_url << std::endl;
        // WebSocket client connection logic goes here.
    }

    // Hosts the local proxy for the browser UI
    void start_local_server(int port = 3000) {
        std::cout << "[LocalGateway] Local UI available at http://localhost:" << port << std::endl;
        // Local HTTP/WS server logic goes here.
    }

    void send_to_cloud(const std::string& payload) {
        // Pushes JSON payload up to the central server
    }

    void send_to_ui(const std::string& payload) {
        // Pushes JSON payload to the local React frontend
    }
};
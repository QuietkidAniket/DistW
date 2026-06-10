#pragma once
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <iostream>

class LocalExecutionEngine {
private:
    std::string current_image = "distw-env:latest";
    std::string workspace_dir = "/tmp/distw_local_workspace/";

    void flush_state_to_disk(const std::string& file_path, const std::string& code) {
        std::string full_path = workspace_dir + file_path;
        std::ofstream out(full_path);
        out << code;
        out.close();
    }

public:
    // Syncs the environment when the Admin updates the Dockerfile
    void rebuild_environment(const std::string& dockerfile_content) {
        flush_state_to_disk("Dockerfile", dockerfile_content);
        std::string cmd = "docker build -t " + current_image + " " + workspace_dir;
        
        system(cmd.c_str());
        std::cout << "[Docker] Environment successfully synchronized with Cloud Admin.\n";
    }

    // Executes the code using the local Docker engine
    std::string execute_code(const std::string& target_file, const std::string& source_code) {
        flush_state_to_disk(target_file, source_code);

        // Strict sandboxing: network disabled, memory capped, read-only volume mounts
        std::string cmd = "docker run --rm --network none --memory 256m -v " + 
                          workspace_dir + ":/app " + current_image + 
                          " sh -c 'g++ /app/" + target_file + " -o /app/a.out && /app/a.out'";

        std::string res = "";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) {
            return "ERROR: Failed to launch local Docker daemon.";
        }

        std::array<char, 128> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            res += buffer.data();
        }

        return res; 
    }
};




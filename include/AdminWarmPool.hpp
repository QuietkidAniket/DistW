#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <memory>
#include <array>

class AdminWarmPool {
private:
    std::string admin_workspace = "/tmp/distw_admin_workspace/";

public:
    //  stdin_data to the execution signature
    std::string execute_code(const std::string& cpp_code, const std::string& stdin_data) {
        std::string workspace_dir = "/tmp/distw_admin_workspace";
        std::string file_path = workspace_dir + "/admin_temp.cpp";
        std::string exec_path = workspace_dir + "/a.out";
        std::string input_path = workspace_dir + "/input.in"; // Path for standard input data

        // 1. Create directory and save C++ source file
        std::filesystem::create_directories(workspace_dir);
        std::ofstream out(file_path);
        if (!out.is_open()) {
            return "\x1b[31m[System Error]\x1b[0m Failed to create workspace file.\n";
        }
        out << cpp_code;
        out.close();

        //  1.5 Save the Standard Input payload to a file
        std::ofstream in_file(input_path);
        if (!in_file.is_open()) {
            return "\x1b[31m[System Error]\x1b[0m Failed to create standard input file.\n";
        }
        in_file << stdin_data;
        in_file.close();

        // Helper lambda to run a command and capture BOTH stdout and stderr
        auto run_cmd = [](const std::string& cmd) -> std::pair<int, std::string> {
            std::string result;
            // The " 2>&1" is the magic that catches cerr and compilation errors!
            FILE* pipe = popen((cmd + " 2>&1").c_str(), "r"); 
            if (!pipe) return {-1, "\x1b[31m[System Error]\x1b[0m popen() failed!\n"};
            
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            int status = pclose(pipe);
            return {status, result};
        };

        // 2. Compile the code (Using clang++ based on my own macOS environment, may replace )
        std::string compile_cmd = "clang++ -std=c++17 " + file_path + " -o " + exec_path;
        auto [comp_status, comp_output] = run_cmd(compile_cmd);

        // If compilation fails, return the errors immediately in red
        if (comp_status != 0) {
            return "\x1b[31m[Compilation Error]\x1b[0m\n" + comp_output;
        }

        //  3. Execute the compiled binary with INPUT REDIRECTION
        // The < operator streams the contents of input.in directly into std::cin
        std::string run_command = exec_path + " < " + input_path;
        auto [run_status, run_output] = run_cmd(run_command);

        // Visual feedback if the program runs but prints nothing
        if (run_output.empty()) {
            return "\x1b[33m[Program finished with no output]\x1b[0m\n";
        }

        return run_output;
    }
};

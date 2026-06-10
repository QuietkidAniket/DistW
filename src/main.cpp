#include <iostream>

// Include all our module headers
#include "SessionManager.hpp"
#include "HierarchicalLockTree.hpp"
#include "DeltaEngine.hpp"
#include "AdminWarmPool.hpp"
#include "ReaperEngine.hpp"
#include "WebSocketRouter.hpp"

int main() {
    std::cout << "========================================\n";
    std::cout << "  DistW Central Cloud Server Starting   \n";
    std::cout << "========================================\n";

    // 1. Initialize the Core Data Structures
    SessionManager session_manager;
    HierarchicalLockTree hdlm;
    
    // The Delta Engine requires a reference to the HDLM for the Security Gate
    DeltaEngine delta_engine(hdlm);
    
    AdminWarmPool admin_pool;

    // 2. Start the Background Garbage Collector
    // This spins up its own std::jthread and runs alongside the main thread
    ReaperEngine reaper(session_manager);

    FileSystemManager fs_manager("./workspace");
    // 3. Initialize the WebSocket Router
    // We inject all the core managers into the router so it can route payloads to them
    WebSocketRouter router(session_manager, hdlm, delta_engine, admin_pool, fs_manager);
    
    // 4. Ignite the Event Loop (This is a blocking call)
    // The server will listen on port 9001 until forcefully terminated
    router.start_server(9001);

    return 0;
}
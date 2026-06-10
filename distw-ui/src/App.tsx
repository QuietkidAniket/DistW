import { useEffect } from 'react';
import { useSocketStore } from './store/useSocketStore';
import EditorWorkspace from './components/EditorWorkspace';
import ExecutionPanel from './components/ExecutionPanel'; 
import FileExplorer from './components/FileExplorer'; 

function App() {
  const { isConnected, connect, disconnect, userId } = useSocketStore();

  useEffect(() => {
    connect(); // Connect now builds the URL internally based on the random user
    return () => {
      disconnect();
    };
  }, [connect, disconnect]);

  return (
    <div className="h-screen flex flex-col bg-[#1e1e1e] text-white overflow-hidden">
      <header className="h-12 border-b border-gray-800 flex items-center px-4 bg-[#252526] justify-between shrink-0">
        <h1 className="text-sm font-mono tracking-tight text-gray-400">
          {/* Display the active user here! */}
          DISTW <span className="text-gray-600">//</span> {userId}
        </h1>
        
        <div className="flex items-center space-x-3">
          <div className={`h-2 w-2 rounded-full ${isConnected ? 'bg-green-500 shadow-[0_0_8px_#22c55e]' : 'bg-red-500'}`} />
          <span className="text-xs font-medium uppercase tracking-widest text-gray-400">
            {isConnected ? 'Sync Active' : 'Offline'}
          </span>
        </div>
      </header>

      <main className="flex-1 flex overflow-hidden w-full">
        <FileExplorer /> 
        <div className="flex-1 flex flex-col min-w-0">
          <EditorWorkspace />
        </div>
        <ExecutionPanel /> 
      </main>
    </div>
  );
}

export default App;
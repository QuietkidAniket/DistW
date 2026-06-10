import { useState } from 'react';
import { useSocketStore } from '../store/useSocketStore';

interface FileNode {
  name: string;
  path: string;
  type: 'file' | 'folder';
  children?: FileNode[];
}

export default function FileExplorer() {
  const { activeLocks, activeFile, setActiveFile, fileTree, createFile } = useSocketStore();
  const [newFileName, setNewFileName] = useState("");
  const [isCreating, setIsCreating] = useState(false);

  const handleCreate = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && newFileName.trim() !== '') {
      console.log("📤 Sending FILE_CREATE packet for:", newFileName.trim()); // 🚨 ADD THIS
      createFile(newFileName.trim());
      setNewFileName("");
      setIsCreating(false);
    } else if (e.key === 'Escape') {
      setIsCreating(false);
    }
  };

  const renderNode = (node: FileNode) => {
    const owner = activeLocks[node.path];
    const isLocked = !!owner;
    const isSelected = activeFile === node.path;

    return (
      <div key={node.path} className="ml-4 mt-1">
        <div 
          onClick={() => node.type === 'file' && setActiveFile(node.path)}
          className={`flex items-center space-x-2 py-1 cursor-pointer rounded px-2 transition-colors ${
            isSelected 
              ? 'bg-blue-900/40 border-l-2 border-blue-500' 
              : 'hover:bg-gray-800 border-l-2 border-transparent'
          }`}
        >
          <span className="text-xs opacity-80">{node.type === 'folder' ? '📁' : '📄'}</span>
          <span className={`text-sm truncate ${isSelected ? 'text-blue-400 font-semibold' : isLocked ? 'text-gray-500' : 'text-gray-300'}`}>
            {node.name}
          </span>
          {isLocked && (
            <span className="text-[10px] bg-red-900/30 text-red-400 px-1 rounded border border-red-800/50 shrink-0 shadow-sm">
              🔒 {owner}
            </span>
          )}
        </div>
        {node.children?.map(renderNode)}
      </div>
    );
  };

  return (
    <div className="w-64 bg-[#252526] border-r border-gray-800 flex flex-col shrink-0 select-none">
      <div className="h-10 flex items-center justify-between px-4 border-b border-gray-800 bg-[#1e1e1e]">
        <span className="text-xs font-bold uppercase tracking-widest text-gray-500">Explorer</span>
        <button 
          onClick={() => setIsCreating(true)}
          className="text-gray-400 hover:text-white transition-colors"
          title="New File"
        >
          + 📄
        </button>
      </div>

      <div className="flex-1 overflow-y-auto py-2">
        {isCreating && (
          <div className="ml-6 flex items-center space-x-2 py-1 px-2">
            <span className="text-xs opacity-80">📄</span>
            <input 
              autoFocus
              type="text"
              value={newFileName}
              onChange={(e) => setNewFileName(e.target.value)}
              onKeyDown={handleCreate}
              onBlur={() => setIsCreating(false)}
              className="bg-[#3c3c3c] text-white text-sm outline-none border border-blue-500 rounded px-1 w-full"
              placeholder="filename.cpp"
            />
          </div>
        )}
        
        {fileTree.map(renderNode)}
        
        {fileTree.length === 0 && !isCreating && (
          <div className="text-center text-xs text-gray-600 mt-10 italic">
            Workspace is empty.
          </div>
        )}
      </div>
    </div>
  );
}
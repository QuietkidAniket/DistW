import { create } from 'zustand';

interface SocketState {
  socket: WebSocket | null;
  isConnected: boolean;
  activeLocks: Record<string, string>; 
  currentBuffer: string;
  activeFile: string;
  
  // NEW: Identity Tracking
  userId: string;
  clientId: string;
  fileTree: any[]; // NEW

  createFile: (filePath: string) => void; // NEW
  
  connect: () => void; // Removed URL argument, we will build it internally
  disconnect: () => void;
  sendDelta: (filePath: string, changes: any[], fullText: string) => void;
  requestLock: (filePath: string, mode: 'SHARED' | 'EXCLUSIVE') => void;
  releaseLock: (filePath: string) => void; 
  executeCode: (filePath: string, code: string) => void;
  setActiveFile: (filePath: string) => void;
  requestFileContent: (filePath: string) => void;
}

export const useSocketStore = create<SocketState>((set, get) => ({
  socket: null,
  isConnected: false,
  activeLocks: {},
  currentBuffer: "",
  activeFile: "src/main.cpp",
  
  // NEW: Generate a random user and client ID on boot
  userId: "user_" + Math.floor(Math.random() * 1000), 
  clientId: Math.random().toString(36).substring(2, 10),
  fileTree: [], // Default to empty

  connect: () => {
    if (get().socket) return;
    const ws = new WebSocket(`ws://localhost:9001/session/alpha/user/${get().userId}`);
    set({ socket: ws });
    
    ws.onopen = () => {
      set({ isConnected: true });
      // Ask the server for the hard drive contents immediately!
      ws.send(JSON.stringify({ type: 'TREE_REQUEST' }));
    };
    
    ws.onclose = () => set({ isConnected: false, socket: null });
    
    ws.onmessage = (event) => {
      try {
        const payload = JSON.parse(event.data);
        switch (payload.type) {
          case 'TREE_SYNC': set({ fileTree: payload.tree || [] }); break; // 🚨 Catch the tree!
          // ... keep all your other cases (LOCK_SYNC, DELTA, etc.) ...
        }
      } catch (err) {}
    };
  },

  // Add the create file function at the bottom:
  createFile: (file_path) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({ type: 'FILE_CREATE', file_path }));
    }
  },

  disconnect: () => {
    const { socket } = get();
    if (socket && socket.readyState === WebSocket.OPEN) socket.close();
    set({ socket: null, isConnected: false, activeLocks: {} });
  },

  sendDelta: (file_path, changes, full_text) => {
    const { socket, clientId } = get();
    if (socket?.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({ 
        type: 'DELTA', 
        client_id: clientId, 
        file_path, 
        changes,
        full_text 
      }));
    }
  },
  requestLock: (file_path, mode) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: 'LOCK_REQUEST', file_path, mode }));
  },

  releaseLock: (file_path) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: 'LOCK_RELEASE', file_path }));
  },

  executeCode: (file_path, code) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: 'ADMIN_RUN', file_path, code }));
  },

  setActiveFile: (file_path) => {
    set({ activeFile: file_path });
    get().requestFileContent(file_path);
  },

  requestFileContent: (file_path) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: 'FILE_REQUEST', file_path }));
  }

}));
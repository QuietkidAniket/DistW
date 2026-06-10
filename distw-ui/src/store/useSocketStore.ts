import { create } from 'zustand';

interface SocketState {
  socket: WebSocket | null;
  isConnected: boolean;
  activeLocks: Record<string, string>; 
  currentBuffer: string;
  activeFile: string;
  
  userId: string;
  clientId: string;
  fileTree: any[]; 

  createFile: (filePath: string) => void; 
  
  connect: () => void; // Removed URL argument, we will build it internally
  disconnect: () => void;
  sendDelta: (filePath: string, changes: any[], fullText: string) => void;
  requestLock: (filePath: string, mode: 'SHARED' | 'EXCLUSIVE') => void;
  releaseLock: (filePath: string) => void; 
  executeCode: (code: string, stdin: string) => void;
  setActiveFile: (filePath: string) => void;
  requestFileContent: (filePath: string) => void;
  deleteFile: (filePath: string) => void; 
  sendCursor: (filePath: string, row: number, col: number) => void;
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
          case 'TREE_SYNC': 
            set({ fileTree: payload.tree || [] }); 
            break;
          case 'LOCK_SYNC': 
            set({ activeLocks: payload.active_locks }); 
            break;
          case 'DELTA': 
            window.dispatchEvent(new CustomEvent('cloud-delta', { detail: payload })); 
            break;
          case 'CURSOR': 
            window.dispatchEvent(new CustomEvent('cursor-update', { detail: payload })); 
            break;
          case 'FILE_CONTENT': 
            window.dispatchEvent(new CustomEvent('file-loaded', { detail: payload })); 
            break;
          case 'RUN_OUTPUT': 
            window.dispatchEvent(new CustomEvent('terminal-output', { detail: payload.data })); 
            break;
          case 'CURSOR': window.dispatchEvent(new CustomEvent('cursor-update', { detail: payload })); break;
        }
      } catch (err) {
        console.error("WebSocket payload error:", err);
      }
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

  executeCode: (code, stdin) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) {
      // Clear previous terminal output locally before running
      window.dispatchEvent(new CustomEvent('terminal-output', { detail: '\x1b[2J\x1b[H' })); 
      
      socket.send(JSON.stringify({ 
        type: 'ADMIN_RUN', 
        code, 
        stdin 
      }));
    }
  },

  setActiveFile: (file_path) => {
    set({ activeFile: file_path });
    get().requestFileContent(file_path);
  },

  requestFileContent: (file_path) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ type: 'FILE_REQUEST', file_path }));
  },

  deleteFile: (file_path) => {
    const { socket } = get();
    if (socket?.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({ type: 'FILE_DELETE', file_path }));
    }
  },
  sendCursor: (file_path, row, col) => {
    const { socket, clientId, userId } = get();
    if (socket?.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({
        type: 'CURSOR',
        client_id: clientId,
        user_id: userId,
        file_path,
        row,
        col
      }));
    }
  }

}));
import { useEffect, useRef, useState, useCallback } from 'react';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';
import { useSocketStore } from '../store/useSocketStore';

export default function ExecutionPanel() {
  const [isOpen, setIsOpen] = useState(false);
  const [width, setWidth] = useState(400); // Default width
  const [stdinData, setStdinData] = useState("");
  
  const terminalRef = useRef<HTMLDivElement>(null);
  const xtermInstance = useRef<Terminal | null>(null);
  const fitAddon = useRef<FitAddon | null>(null);
  const isResizing = useRef(false);

  const { activeFile, currentBuffer, executeCode } = useSocketStore();

  // --- Drag to Resize Logic ---
  const startResizing = useCallback(() => {
    isResizing.current = true;
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none'; // Prevent text selection while dragging
  }, []);

  const stopResizing = useCallback(() => {
    isResizing.current = false;
    document.body.style.cursor = 'default';
    document.body.style.userSelect = '';
  }, []);

  const resize = useCallback((e: MouseEvent) => {
    if (isResizing.current) {
      // Calculate new width based on window size minus mouse position
      const newWidth = window.innerWidth - e.clientX;
      if (newWidth > 250 && newWidth < 800) {
        setWidth(newWidth);
        fitAddon.current?.fit(); // Auto-resize terminal grid
      }
    }
  }, []);

  useEffect(() => {
    window.addEventListener("mousemove", resize);
    window.addEventListener("mouseup", stopResizing);
    return () => {
      window.removeEventListener("mousemove", resize);
      window.removeEventListener("mouseup", stopResizing);
    };
  }, [resize, stopResizing]);

  // --- XTerm.js Initialization ---
  useEffect(() => {
    if (!terminalRef.current || !isOpen) return;

    xtermInstance.current = new Terminal({
      theme: { background: '#1e1e1e' },
      cursorBlink: true,
      fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
      fontSize: 13,
    });
    
    fitAddon.current = new FitAddon();
    xtermInstance.current.loadAddon(fitAddon.current);
    xtermInstance.current.open(terminalRef.current);
    fitAddon.current.fit();

    xtermInstance.current.writeln('\x1b[38;5;51mDistW Edge Execution Engine Initialized.\x1b[0m\r\n');

    const handleOutput = (e: Event) => {
      const customEvent = e as CustomEvent<string>;
      // Format line breaks properly for xterm
      const formatted = customEvent.detail.replace(/\n/g, '\r\n');
      xtermInstance.current?.write(formatted);
    };

    window.addEventListener('terminal-output', handleOutput);

    return () => {
      window.removeEventListener('terminal-output', handleOutput);
      xtermInstance.current?.dispose();
    };
  }, [isOpen]);

  const handleRun = () => {
    if (activeFile && currentBuffer) {
      xtermInstance.current?.writeln(`\x1b[33m> Compiling & Executing ${activeFile}...\x1b[0m\r\n`);
      executeCode(currentBuffer, stdinData);
    }
  };

  // --- Collapsed State ---
  if (!isOpen) {
    return (
      <div className="bg-[#252526] border-l border-gray-800 flex flex-col items-center pt-4 w-14 shrink-0 transition-all">
        <button 
          onClick={() => setIsOpen(true)}
          className="text-gray-400 hover:text-white hover:bg-gray-800 p-2 rounded transition-colors"
          title="Open Execution Panel"
        >
          {/* Clean Terminal Icon instead of sideways text */}
          <svg xmlns="http://www.w3.org/2000/svg" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="4 17 10 11 4 5"></polyline>
            <line x1="12" y1="19" x2="20" y2="19"></line>
          </svg>
        </button>
      </div>
    );
  }

  // --- Expanded State ---
  return (
    <div 
      style={{ width: `${width}px` }} 
      className="bg-[#1e1e1e] border-l border-gray-800 flex flex-col shrink-0 relative"
    >
      {/* The invisible drag handle on the left edge */}
      <div 
        onMouseDown={startResizing}
        className="absolute left-0 top-0 bottom-0 w-1.5 cursor-col-resize hover:bg-blue-500/50 z-50 transition-colors"
      />

      <div className="h-10 border-b border-gray-800 flex items-center justify-between px-4 shrink-0 bg-[#252526]">
        <span className="text-xs font-bold uppercase tracking-widest text-gray-400">Execution Panel</span>
        <div className="flex space-x-3">
          <button 
            onClick={handleRun}
            className="text-xs bg-green-600 hover:bg-green-500 text-white px-3 py-1 rounded shadow-md font-medium flex items-center space-x-1"
          >
            <span>▶</span> <span>Run Code</span>
          </button>
          <button 
            onClick={() => setIsOpen(false)}
            className="text-gray-400 hover:text-white"
          >
            ✕
          </button>
        </div>
      </div>

      {/* Input Pane (Standard Input) */}
      <div className="h-1/3 border-b border-gray-800 flex flex-col bg-[#1e1e1e]">
        <div className="px-3 py-1 bg-[#2d2d2d] border-b border-gray-800 text-[10px] text-gray-400 font-mono uppercase tracking-wider">
          Standard Input (input.in)
        </div>
        <textarea 
          value={stdinData}
          onChange={(e) => setStdinData(e.target.value)}
          placeholder="Paste your standard input (test cases) here..."
          className="flex-1 bg-transparent text-gray-300 font-mono text-sm p-3 outline-none resize-none"
          spellCheck={false}
        />
      </div>

      {/* Output Pane (XTerm.js) */}
      <div className="flex-1 flex flex-col min-h-0 bg-[#1e1e1e]">
        <div className="px-3 py-1 bg-[#2d2d2d] border-b border-gray-800 text-[10px] text-gray-400 font-mono uppercase tracking-wider">
          Terminal Output (stdout)
        </div>
        <div className="flex-1 p-2 overflow-hidden" ref={terminalRef} />
      </div>
    </div>
  );
}
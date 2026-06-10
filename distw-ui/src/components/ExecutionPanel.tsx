import { useEffect, useRef } from 'react';
import { Terminal } from '@xterm/xterm';
import '@xterm/xterm/css/xterm.css';
import { useSocketStore } from '../store/useSocketStore';

export default function ExecutionPanel() {
  const terminalRef = useRef<HTMLDivElement>(null);
  const xtermRef = useRef<Terminal | null>(null);

  useEffect(() => {
    if (!terminalRef.current) return;
    
    // 1. Initialize Terminal
    const term = new Terminal({
      theme: { background: '#1e1e1e', cursor: '#ffffff', selectionBackground: '#5a5d5e' },
      fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
      fontSize: 13,
      convertEol: true, 
    });
    
    term.open(terminalRef.current);
    term.writeln('\x1b[34m[DistW Terminal]\x1b[0m Connected to secure sandbox.');
    term.write('\x1b[32muser@cloud-edge:~/workspace $\x1b[0m ');
    
    xtermRef.current = term;

    // 2. Define the listener
    const handleTerminalOutput = (e: any) => {
      const output = e.detail;
      if (term) {
        // Write the raw output from the C++ server
        term.write('\r\n' + output);
        // Reset the prompt
        term.write('\r\n\x1b[32muser@cloud-edge:~/workspace $\x1b[0m ');
      }
    };

    // 3. Attach to window
    window.addEventListener('terminal-output', handleTerminalOutput);

    // 4. Cleanup
    return () => {
      window.removeEventListener('terminal-output', handleTerminalOutput);
      term.dispose();
    };
  }, []);

  const handleRun = () => {
    if (!xtermRef.current) return;
    
    xtermRef.current.writeln('\r\n\x1b[33m[System] Sending build request to edge...\x1b[0m');
    
    const code = useSocketStore.getState().currentBuffer;
    useSocketStore.getState().executeCode("src/main.cpp", code);
  };

  return (
    <div className="w-[450px] bg-[#1e1e1e] flex flex-col border-l border-gray-800 shrink-0">
      <div className="h-10 bg-[#252526] border-b border-gray-800 flex items-center px-4 justify-between shrink-0">
        <div className="flex items-center space-x-2">
          <span className="text-[10px] bg-blue-500/20 text-blue-400 px-1.5 py-0.5 rounded border border-blue-500/30">BASH</span>
          <span className="text-xs text-gray-400 uppercase tracking-wider font-semibold">Terminal</span>
        </div>
        
        <button 
          onClick={handleRun}
          className="text-xs bg-green-600 hover:bg-green-500 text-white px-4 py-1 rounded font-bold shadow-lg transition-all active:scale-95"
        >
          Execute Code
        </button>
      </div>

      <div className="flex-1 p-2 overflow-hidden bg-[#1e1e1e] font-mono" ref={terminalRef} />
    </div>
  );
}
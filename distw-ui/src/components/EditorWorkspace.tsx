import { useEffect, useRef } from 'react';
import Editor from '@monaco-editor/react';
import { useSocketStore } from '../store/useSocketStore';
const DEFAULT_CPP_TEMPLATE = `#include <iostream>

int main() {
  std::cout << "DistW Cloud Environment Initialized." << std::endl;
  return 0;
  }
  `;
  
  export default function EditorWorkspace() {
  const { activeFile, activeLocks, requestLock, releaseLock, userId } = useSocketStore(); // Pull userId
  
  const editorRef = useRef<any>(null);
  const monacoRef = useRef<any>(null); 
  const isRemoteUpdate = useRef(false); 
  
  const currentOwner = activeLocks[activeFile];
  const hasLock = currentOwner === userId; 
  const isLockedByOther = !!currentOwner && !hasLock;

  // 1. Initial boot-up file request
  useEffect(() => {
    // Force the server to send the file state the moment the editor mounts
    useSocketStore.getState().setActiveFile(activeFile);
  }, []);

  useEffect(() => {
    const handleFileLoaded = (e: Event) => {
      const customEvent = e as CustomEvent;
      const { file_path, content } = customEvent.detail;
      
      if (file_path === activeFile && editorRef.current) {
        // 🚨 PREVENT ECHO LOOP: Lock the editor before injecting server code
        isRemoteUpdate.current = true;
        
        const textToSet = content && content.length > 0 ? content : DEFAULT_CPP_TEMPLATE;
        editorRef.current.setValue(textToSet);
        useSocketStore.setState({ currentBuffer: textToSet });
        
        // Unlock after a tiny delay
        setTimeout(() => { isRemoteUpdate.current = false; }, 50);
      }
    };

    const handleRemoteDelta = (e: Event) => {
      const customEvent = e as CustomEvent;
      
      // We now pull full_text from the packet too!
      const { file_path, changes, full_text, client_id } = customEvent.detail;
      
      if (client_id === useSocketStore.getState().clientId) return;
      if (file_path !== activeFile || !editorRef.current || !monacoRef.current) return;

      isRemoteUpdate.current = true;
      
      const model = editorRef.current.getModel();
      const m = monacoRef.current;

      const edits = changes.map((c: any) => ({
        range: new m.Range(
          c.range.startLineNumber, 
          c.range.startColumn, 
          c.range.endLineNumber, 
          c.range.endColumn
        ),
        text: c.text
      }));

      // 1. Inject directly into the data model (Bypasses the Read-Only UI shield instantly!)
      model.applyEdits(edits);

      // 2. THE SELF-HEALING CHECK
      // If Monaco's delta math ever messes up (like the abcddcba bug), 
      // we instantly snap the text to match the C++ server's master copy.
      if (model.getValue() !== full_text) {
        console.warn("Delta desync detected! Self-healing document...");
        
        // We use applyEdits on the full range instead of setValue() 
        // so Tab 2's scrollbar doesn't jump to the top!
        model.applyEdits([{
          range: model.getFullModelRange(),
          text: full_text
        }]);
      }

      setTimeout(() => { isRemoteUpdate.current = false; }, 50);
    };

    window.addEventListener('file-loaded', handleFileLoaded);
    window.addEventListener('cloud-delta', handleRemoteDelta);

    return () => {
      window.removeEventListener('file-loaded', handleFileLoaded);
      window.removeEventListener('cloud-delta', handleRemoteDelta);
    };
  }, [activeFile]);

  const handleEditorMount = (editor: any, monaco: any) => {
    editorRef.current = editor;
    monacoRef.current = monaco; 

    editor.onDidChangeModelContent((event: any) => {
      if (isRemoteUpdate.current) return; 

      const store = useSocketStore.getState();
      const currentActiveFile = store.activeFile;
      const currentlyHasLock = store.activeLocks[currentActiveFile] === store.userId;

      store.currentBuffer = editor.getValue();

      if (!currentlyHasLock) return;
      
      store.sendDelta(currentActiveFile, event.changes, editor.getValue());
    });
  };

  return (
    <div className="flex-1 flex flex-col min-w-0 bg-[#1e1e1e]">
      <div className="h-10 bg-[#1e1e1e] border-b border-gray-800 flex items-center px-4 justify-between shrink-0">
        <div className="flex items-center space-x-3">
          <span className="text-sm text-blue-400 font-mono">{activeFile.split('/').pop()}</span>
          
          {hasLock && <span className="text-xs bg-green-900/50 text-green-400 px-2 py-0.5 rounded border border-green-800 shadow-[0_0_10px_rgba(34,197,94,0.2)]">EDITING</span>}
          {isLockedByOther && <span className="text-xs bg-red-900/50 text-red-400 px-2 py-0.5 rounded border border-red-800">LOCKED BY {currentOwner}</span>}
        </div>
        
        <div className="flex space-x-2">
          {!hasLock && (
            <button 
              onClick={() => requestLock(activeFile, 'EXCLUSIVE')}
              disabled={isLockedByOther}
              className={`text-xs px-3 py-1 rounded font-medium transition-all ${
                isLockedByOther 
                  ? 'bg-gray-800 text-gray-500 cursor-not-allowed' 
                  : 'bg-blue-600 hover:bg-blue-500 text-white shadow-md active:scale-95'
              }`}
            >
              Acquire Write Lock
            </button>
          )}

          {hasLock && (
            <button 
              onClick={() => releaseLock(activeFile)}
              className="text-xs px-3 py-1 rounded font-medium transition-all bg-gray-700 hover:bg-red-600 text-white shadow-md active:scale-95"
            >
              Release Lock
            </button>
          )}
        </div>
      </div>

      <div className="flex-1 overflow-hidden relative">
        <Editor
          height="100%"
          defaultLanguage="cpp"
          theme="vs-dark"
          options={{
            minimap: { enabled: false },
            fontSize: 14,
            fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
            wordWrap: 'on',
            readOnly: !hasLock, // Enforces actual typing block when unlocked
            padding: { top: 16 },
            scrollBeyondLastLine: false,
            cursorBlinking: hasLock ? 'blink' : 'solid',
            cursorStyle: 'line',
          }}
          onMount={handleEditorMount}
        />
        
        {/* Visual Read-Only Overlay to guarantee they can't click into it */}
        {!hasLock && (
          <div className="absolute inset-0 z-10 bg-black/10 cursor-not-allowed" title="Acquire lock to edit" />
        )}
      </div>
    </div>
  );
}
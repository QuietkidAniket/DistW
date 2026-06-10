import { useEffect, useRef } from 'react';
import Editor from '@monaco-editor/react';
import { useSocketStore } from '../store/useSocketStore';

const DEFAULT_CPP_TEMPLATE = `#include <iostream>

int main() {
  std::cout << "DistW Cloud Environment Initialized." << std::endl;
  return 0;
}
`;

// the vertical caret line 
if (typeof document !== 'undefined') {
  const style = document.createElement('style');
  style.innerHTML = `
    /* The vertical caret line */
    .remote-caret {
      border-left: 2px solid #ec4899 !important;
    }
  `;
  document.head.appendChild(style);
}

export default function EditorWorkspace() {
  const { activeFile, activeLocks, requestLock, releaseLock, userId } = useSocketStore();
  
  const editorRef = useRef<any>(null);
  const monacoRef = useRef<any>(null); 
  const isRemoteUpdate = useRef(false); 
  
  const remoteCursorsRef = useRef<Record<string, { row: number; col: number; user_id: string; file_path: string }>>({});
  const cursorCollectionRef = useRef<any>(null);

  const currentOwner = activeLocks[activeFile];
  const hasLock = currentOwner === userId; 
  const isLockedByOther = !!currentOwner && !hasLock;

  useEffect(() => {
    useSocketStore.getState().setActiveFile(activeFile);
  }, []);

  useEffect(() => {
    if (editorRef.current) {
      editorRef.current.updateOptions({ readOnly: !hasLock });
      if (hasLock) {
        editorRef.current.focus();
      }
    }
  }, [hasLock]);

  useEffect(() => {
    const handleFileLoaded = (e: Event) => {
      const customEvent = e as CustomEvent;
      const { file_path, content } = customEvent.detail;
      
      if (file_path === activeFile && editorRef.current) {
        isRemoteUpdate.current = true;
        const textToSet = content && content.length > 0 ? content : DEFAULT_CPP_TEMPLATE;
        editorRef.current.setValue(textToSet);
        useSocketStore.setState({ currentBuffer: textToSet });
        setTimeout(() => { isRemoteUpdate.current = false; }, 50);
      }
    };

    const handleRemoteDelta = (e: Event) => {
      const customEvent = e as CustomEvent;
      const { file_path, changes, full_text, client_id } = customEvent.detail;
      
      const store = useSocketStore.getState();

      // 1. Standard Client ID check
      if (client_id === store.clientId) return;
      
      // 2. The Lock Ownership Check:
      // If I am the active owner of this file's lock, completely ignore incoming deltas.
      // I am the sole writer, so any incoming packet is just an old echo of my own fast typing!
      if (store.activeLocks[file_path] === store.userId) return;

      // 3. Ensure we only process deltas for the tab we are currently viewing
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

      model.applyEdits(edits);

      if (model.getValue() !== full_text) {
        console.warn("Delta desync detected! Self-healing document...");
        model.applyEdits([{
          range: model.getFullModelRange(),
          text: full_text
        }]);
      }

      setTimeout(() => { isRemoteUpdate.current = false; }, 50);
    };

    const handleRemoteCursor = (e: Event) => {
      const customEvent = e as CustomEvent;
      const { file_path, row, col, client_id, user_id } = customEvent.detail;
      
      if (client_id === useSocketStore.getState().clientId) return;
      if (!editorRef.current || !monacoRef.current || !cursorCollectionRef.current) return;

      remoteCursorsRef.current[client_id] = { row, col, user_id, file_path };

      const m = monacoRef.current;
      const decorationsData: any[] = [];

      Object.values(remoteCursorsRef.current).forEach((cursor) => {
        if (cursor.file_path === activeFile) {
          // 🚨 THE FIX: Draw ONLY the vertical caret (No full-width line background highlights)
          decorationsData.push({
            range: new m.Range(cursor.row, cursor.col, cursor.row, cursor.col + 1),
            options: {
              className: 'remote-caret',
              stickiness: m.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges
            }
          });
        }
      });

      cursorCollectionRef.current.set(decorationsData);
    };

    window.addEventListener('file-loaded', handleFileLoaded);
    window.addEventListener('cloud-delta', handleRemoteDelta);
    window.addEventListener('cursor-update', handleRemoteCursor);

    return () => {
      window.removeEventListener('file-loaded', handleFileLoaded);
      window.removeEventListener('cloud-delta', handleRemoteDelta);
      window.removeEventListener('cursor-update', handleRemoteCursor);
    };
  }, [activeFile]);

  const handleEditorMount = (editor: any, monaco: any) => {
    editorRef.current = editor;
    monacoRef.current = monaco; 

    cursorCollectionRef.current = editor.createDecorationsCollection([]);

    editor.onDidChangeCursorPosition((event: any) => {
      // If the user's blinking text cursor is not physically focused inside this specific editor, 
      // completely ignore the movement. This prevents background tab loads from broadcasting (1,1).
      if (!editor.hasTextFocus()) {
        return; 
      }

      const store = useSocketStore.getState();
      if (store.activeFile) {
        store.sendCursor(store.activeFile, event.position.lineNumber, event.position.column);
      }
    });

    editor.onDidChangeCursorPosition((event: any) => {
      const store = useSocketStore.getState();
      if (store.activeFile) {
        store.sendCursor(store.activeFile, event.position.lineNumber, event.position.column);
      }
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
            readOnly: !hasLock,
            padding: { top: 16 },
            scrollBeyondLastLine: false,
            cursorBlinking: hasLock ? 'blink' : 'solid',
            cursorStyle: 'line',
            renderLineHighlight: 'none', // Completely turns off Monaco's native background bar highlight under the person typing
          }}
          onMount={handleEditorMount}
        />
        
        {!hasLock && (
          <div className="absolute inset-0 z-10 bg-black/10 cursor-not-allowed" title="Acquire lock to edit" />
        )}
      </div>
    </div>
  );
}
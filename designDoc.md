# Custom Myterminal DesignDoc file
Subject: **Computing Lab(CS69201)**.  
Name :   **Narendra Kumar (25CS60R18).**
---

## 1. Overview

**MyTerm** is a custom 2D terminal emulator built using **X11/Xlib**.  
It provides a fully functional shell environment with GUI, multiple tabs, Unicode input, command history, auto-completion, pipes, redirection, and parallel command monitoring using **MultiWatch**.

It blends **low-level X11 graphics** with **POSIX system programming** to simulate a lightweight yet modern terminal.

---

## 2. **ARCHITECTURE**
### 2.1 System Layers


- **Presentation Layer (X11 GUI)** : Window creation, rendering, event handling, fonts, tab bar 
- **Shell Logic Layer** : Command parsing, execution, piping, I/O redirection, history |
- **Process Management Layer** : fork/exec, signal handling, pipes, IPC, file descriptors 

### 2.2 Data Structures

#### Tab Structure – State per terminal tab

| Key | Description |
|-----|-------------|
| `cDir` | Current working directory |
| `oBuf` | Output buffer (vector of strings) |
| `iBuf` | Input buffer (active user-typed command) |
| `sY` | Scroll position of output |
| `hist` | Command history (max 10,000) |
| `hIdx` | Current history pointer |
| `cPos` | Cursor position in input buffer |
| `sMode` | Search mode (Ctrl+R) flag |
| `sBuf` | Search input text |
| `tabSelMode` | Tab-completion selection active |
| `tabMatches` | List of matches for auto-completion |
| `tabPrefix` | Original prefix for completion |
| `tabLastSpace` | Last space index in command |



**MyTerm Class**
Main application controller containing:  
  
Display and window handles  
Graphics context  
Font structures (ASCII and FontSet for Unicode)  
Color definitions  
Tab collection  
Current tab index  
Foreground process ID  
MultiWatch state  

## 3. FEATURE IMPLEMENTATION

- **Graphical User Interface (X11)**: Custom terminal window with multiple tabs, text buffer, input/output through X11. Handles keyboard/mouse events, redraws UI, supports Unicode via XmbDrawString, and independent shell per tab using fork + execvp.  

- **Command Execution**: External commands run in child processes with pipes capturing stdout/stderr. Built-in commands (cd, clear, history, exit) handled directly.  

- **I/O Redirection**: Supports `<` (input) and `>` (output), combined redirection, using dup2 to map file descriptors.  

- **Pipe Support**: Implements `|` to chain commands. Forks multiple processes and links via pipe() to pass output sequentially.  

- **multiWatch Feature**: Executes multiple commands in parallel. Each command writes to hidden temp files; parent monitors with select() and displays output with timestamps. Handles Ctrl+C cleanup, killing child processes and deleting temp files.  

- **Unicode Input & Font Rendering**: Multiline UTF-8 input via XIM/XIC. Uses XFontSet with DejaVu/Noto/Lohit fonts for proper rendering and locale set to UTF-8.  

- **Command History**: Stores up to 10,000 commands in `~/.myterm_history`. Supports navigation with arrows and Ctrl+R incremental search with exact or substring match.  

- **Tab Auto-Completion**: Tab key triggers filename completion. Single match auto-completes; multiple matches show common prefix or numbered selection (1–9).  

- **Cursor Navigation**: Ctrl+A moves to start, Ctrl+E to end. Arrow keys adjust position; visual cursor drawn with green rectangle.  

- **Signal Handling**: Ctrl+C interrupts foreground process or multiWatch; Ctrl+Z suspends foreground process.  

- **Tab Management**: Create (Ctrl+T) and close (Ctrl+W) tabs, switch with Ctrl+1–9 or Ctrl+Tab. Each tab maintains independent working directory, output buffer, and input state.  

## 4. DESIGN DECISIONS
- **Single-Threaded Event Loop**: Simple sequential processing; concurrency via child processes.  
- **Pipe-Based Output Capture**: Child processes output captured through pipes.  
- **In-Memory Buffer**: Fast scrolling and rendering; size limits optional.  
- **FontSet & Manual Rendering**: X11 FontSet for simplicity; custom text layout for control and learning.  
- **History File**: Plain text, human-readable, easy to edit; lacks metadata.

## 5. SYSTEM CALLS USED
- **Process Management**: fork, execvp, waitpid, getpid  
- **Signal Handling**: kill, SIGINT, SIGTSTP, SIGKILL  
- **File Operations**: open, read, write, close, dup2, pipe  
- **I/O Multiplexing**: select, FD_ZERO, FD_SET  
- **File System**: chdir, opendir/readdir/closedir, unlink  
- **Other**: sleep, time/ctime, lseek


## 6. LIMITATIONS AND FUTURE ENHANCEMENTS
| Category | Details |
|----------|---------|
| **Tab Completion** | Only current directory, max 9 choices, no command name completion |
| **Command Substitution** | Backticks and nested commands not supported |
| **Redirection** | No append, here-docs, or fd redirection |
| **Terminal Emulation** | No ANSI sequences, colors, cursor positioning, or interactive programs |
| **Text Rendering** | No line wrap or mouse selection; clipboard support can be added |
| **Process Management** | Job control, background tracking, foregrounding, job status to be added |
| **Configuration** | Colors, fonts, shortcuts, window size can be configured |
| **Command Features** | Env variables, aliases, shell functions, arithmetic expansion |
| **Tabs** | Drag/drop, split panes, tab groups/workspaces possible |
| **History** | Incremental search, deduplication, timestamps, cross-tab persistence |
| **Performance** | Render visible lines only, buffer limits, optimize font measurement |


## 7. TESTING APPROACH
| Testing Type | Details |
|--------------|---------|
| **Unit Testing** | Test individual features: command parsing, tab completion, history search, I/O redirection |
| **Integration Testing** | Test features together: pipes, redirection, MultiWatch, tab operations, history across tabs |
| **User Testing** | Manual testing: typing speed, Unicode input, tab completion, visual appearance |
| **Edge Cases** | Test special scenarios: empty/long commands, many tabs, large outputs, nonexistent files, failed commands, signals |

## 8. COMPILATION AND DEPLOYMENT

**Dependencies**  
- libX11-dev  
- C++17 compiler  
- POSIX-compliant system  

**Build Command**  
`bash
g++ -o myterm myterm.cpp -lX11 -std=c++17
`   
**Runtime Requirements**  
-X11 server running with DISPLAY set  
-Unicode fonts installed  
HOME environment variable for history

**Installation**  
No installation needed  
Single executable can run from any directory  
History file auto-created in user home

## 9. CODE ORGANIZATION
Constants
Window dimensions, margins, colors defined as constexpr.  
Tab Structure
Encapsulates all state for one terminal session.  
MyTerm Class
Main application controller with private helper methods and public interface.

**Helper Methods**  
initDisp: X11 initialization  
ldHist, svHist: history management  
trim: string utility  
mkPmt: generate prompt string  
exCmd, exPipeCmd, exMultiWatch: command execution  
hndlCtrlR, hndlTabCmp, hndlTabSelection: special features  
lngCmnPfx: common prefix calculation  
hndlKeyPr, hndlMouse, hndlSel: event handlers  
draw: rendering

**Main Function**  
Creates MyTerm instance and calls run method which enters event loop.

## 10. CONCLUSION
- MyTerm demonstrates a functional terminal emulator built with low-level system programming.  
- Combines X11 GUI, POSIX system calls, process management, and shell parsing.  
- Architecture separates presentation, logic, and process management for clarity and learning.  
- Supports Unicode, tab completion, history search, pipes, redirection, and a custom multiWatch utility.  
- Provides a solid foundation for understanding terminal emulators and can be extended with more features.

Custom Myterminal readme file
--- 
Subject: **Computing Lab(CS69201)**.  
Name :   **Narendra Kumar.**
---

## System requirements (short)
- **Libraries:** X11 (Xlib)
- **Compiler:** C++17 (g++)
- **POSIX APIs:** fork, execvp, pipe, dup2, select

**Install (Ubuntu/Debian)**
```bash
sudo apt-get install libx11-dev 
```

**Build**
```bash
# inside folder having project file
g++ -o mytermproj mytermproj.cpp -lX11 -std=c++17
```

**Run**
```bash
./mytermproj
```

---

## Usage (cheat-sheet)
- `cd /path/to/dir` — change directory
- `ls -la` — list files
- `gcc -o program program.c && ./program` — compile & run
- `clear` — clear screen
- `history` — show recent commands
- `exit` or `close` — quit

**Redirection examples**
```bash
sort < unsorted.txt
ls -l > filelist.txt
./myprog < in.txt > out.txt
```

**Pipes**
```bash
ls *.txt | wc -l
cat names.txt | sort | more
ps aux | grep firefox | awk '{print $2}'
```

**multiWatch**
```bash
multiWatch ["date", "uptime", "free -h"]
# stop with Ctrl+C
```

**Unicode**
```bash
echo "नमस्ते दुनिया"
echo "Hello World"
```

---

## Keyboard shortcuts (quick)
```
Ctrl+A   Move to start of line
Ctrl+E   Move to end of line
Ctrl+R   Reverse history search
Ctrl+C   Interrupt running command
Ctrl+Z   Suspend command
Ctrl+T   New tab
Ctrl+W   Close tab
Ctrl+1-9 Switch to tab N
Tab      Filename auto-complete
Up/Down  Browse history
Esc      Cancel selection
```
---

## Troubleshooting (common)
- **Cannot open display** → check `$DISPLAY` or `export DISPLAY=:0`
- **Fonts missing** → `fc-list | grep -i devanagari` → install Noto/Lohit
- **History not saving** → check `echo $HOME` and file permissions
- **Unicode not rendering** → `export LC_ALL=en_US.UTF-8; export LANG=en_US.UTF-8`

---

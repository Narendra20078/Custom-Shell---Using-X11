#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <filesystem>
#include <cstdlib>
#include <signal.h>
#include <sys/select.h>
#include <ctime>
#include <sstream>
#include <dirent.h>
#include <algorithm>

using namespace std;
namespace fs = filesystem;

constexpr int WIN_WIDTH = 1000;
constexpr int WIN_HEIGHT = 700;
constexpr int LINE_HEIGHT = 20;
constexpr int LEFT_MARGIN = 10;
constexpr int TAB_HEIGHT = 30;
constexpr int TAB_WIDTH = 100;
constexpr int TAB_SPACING = 5;
constexpr int BOTTOM_MARGIN = 60;

struct Tab
{
    string cDir;        
    vector<string> oBuf;
    string iBuf;        
    int sY;             
    vector<string> hist;
    int hIdx;     
    int cPos;     
    bool sMode;   
    string sBuf;  
    string selTxt;

    bool tabSelMode;
    vector<string> tabMatches;
    string tabPrefix;
    size_t tabLastSpace;

    Tab();
};

class MyTerm
{
private:
    Display *disp;
    Window wnd;
    GC gc;
    XFontStruct *fnt;
    XFontSet fntset;
    XIM xim;
    XIC xic;
    Colormap cmap;
    XColor wCol, bgCol, pCol, gCol, dCol, rCol;
    int scrn;
    int wW, wH;
    bool rdraw;

    vector<Tab> tabs;
    int cTab;

    pid_t fgPid; 
    bool mwActive; 
    vector<pid_t> mwPids; 

    string hFile;

    void initDisp();
    void ldHist(Tab &tb);
    void svHist(const string &cmd);
    string trim(const string &s);
    string mkPmt(const Tab &tb);
    void exCmd(const string &cmdRaw, Tab &tb);
    void exPipeCmd(const string &cmdRaw, Tab &tb);
    void exMultiWatch(const string &cmdRaw, Tab &tb);
    void hndlCtrlR(Tab &tb);
    void hndlTabCmp(Tab &tb);
    void hndlTabSelection(Tab &tb, char input);
    string lngCmnPfx(const vector<string> &strs);
    void hndlKeyPr(XEvent &ev);
    void hndlMouse(XEvent &ev);
    void hndlSel(XEvent &ev);
    void draw();

public:
    MyTerm();
    ~MyTerm();
    void run();
};

Tab::Tab()
    : cDir(fs::current_path().string()),
      sY(0),
      hIdx(-1),
      cPos(0),
      sMode(false),
      sBuf(""),
      selTxt(""),
      tabSelMode(false),
      tabPrefix(""),
      tabLastSpace(string::npos)
{}

MyTerm::MyTerm()
    : disp(nullptr),
      gc(0),
      fnt(nullptr),
      fntset(nullptr),
      xim(nullptr),
      xic(nullptr),
      wW(WIN_WIDTH),
      wH(WIN_HEIGHT),
      rdraw(true),
      cTab(0),
      fgPid(-1),
      mwActive(false)
{
    setlocale(LC_ALL, "en_US.UTF-8");
    tabs.emplace_back();
    ldHist(tabs[0]);

    if (!isatty(fileno(stdin)))
    {
        Tab tb;
        tabs.push_back(tb);
        string line;
        while (getline(cin, line))
        {
            tb.oBuf.push_back(mkPmt(tb) + line);
            exCmd(line, tb);
        }
        for (auto &l : tb.oBuf)
            cout << l << "\n";
        _exit(0);
    }

    initDisp();
}

MyTerm::~MyTerm()
{
    if (xic)
        XDestroyIC(xic);
    if (xim)
        XCloseIM(xim);
    if (fntset)
        XFreeFontSet(disp, fntset);
    if (fnt)
        XFreeFont(disp, fnt);
    XFreeGC(disp, gc);
    XDestroyWindow(disp, wnd);
    XCloseDisplay(disp);
}

void MyTerm::initDisp()
{
    disp = XOpenDisplay(nullptr);
    if (!disp)
    {
        cerr << "Cannot open display\n";
        _exit(1);
    }

    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");

    scrn = DefaultScreen(disp);

    wnd = XCreateSimpleWindow(disp, RootWindow(disp, scrn),
                              10, 10, WIN_WIDTH, WIN_HEIGHT, 1,
                              BlackPixel(disp, scrn),
                              BlackPixel(disp, scrn));

    XSelectInput(disp, wnd, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);

    XStoreName(disp, wnd, "MyTerminal");
    XMapWindow(disp, wnd);

    gc = XCreateGC(disp, wnd, 0, nullptr);

    xim = XOpenIM(disp, nullptr, nullptr, nullptr);
    if (!xim)
    {
        cerr << "Warning: Cannot open Input Method for multibyte input\n";
    }
    else
    {
        xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, wnd, XNFocusWindow, wnd, nullptr);
        if (!xic)
            cerr << "Warning: Cannot create Input Context\n";
        else
            cerr << "Input Method initialized successfully for Hindi/Marathi input\n";
    }

    char **missingCharsets;
    int numMissing;
    char *defString;

    fntset = XCreateFontSet(disp,
                            "-*-dejavu sans-medium-r-normal-*-16-*-*-*-*-*-*-*,"
                            "-*-noto sans devanagari-medium-r-normal-*-16-*-*-*-*-*-*-*,"
                            "-*-lohit devanagari-medium-r-normal-*-16-*-*-*-*-*-*-*,",
                            &missingCharsets, &numMissing, &defString);

    if (!fntset)
    {
        cerr << "Warning: Cannot load fontset for Hindi/Marathi. Falling back to ASCII font.\n";
        fnt = XLoadQueryFont(disp, "-misc-fixed-medium-r-normal--16-150-75-75-c-70-iso10646-1");
        if (!fnt)
        {
            cerr << "Cannot load fallback font\n";
            _exit(1);
        }
        XSetFont(disp, gc, fnt->fid);
    }
    else
    {
        cerr << "Fontset loaded successfully for multibyte text support\n";
        if (numMissing > 0)
        {
            cerr << "Some charsets missing: ";
            for (int i = 0; i < numMissing; i++)
                cerr << missingCharsets[i] << " ";
            cerr << "\n";
            XFreeStringList(missingCharsets);
        }
    }

    cmap = DefaultColormap(disp, scrn);
    XParseColor(disp, cmap, "#FFFFFF", &wCol);
    XAllocColor(disp, cmap, &wCol);
    XParseColor(disp, cmap, "#ff0000", &bgCol);
    XAllocColor(disp, cmap, &bgCol);
    XParseColor(disp, cmap, "#19f819", &pCol);
    XAllocColor(disp, cmap, &pCol);
    XParseColor(disp, cmap, "#d5a45a", &gCol);
    XAllocColor(disp, cmap, &gCol);
    XParseColor(disp, cmap, "#435344", &dCol);
    XAllocColor(disp, cmap, &dCol);
    XParseColor(disp, cmap, "#800000", &rCol);
    XAllocColor(disp, cmap, &rCol);
}

void MyTerm::ldHist(Tab &tb)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        cerr << "Warning: HOME environment variable not set. History disabled.\n";
        return;
    }

    hFile = string(home) + "/.myterm_history";
    ifstream inf(hFile);

    if (inf.is_open())
    {
        string line;
        int cnt = 0;
        while (getline(inf, line))
            if (!line.empty())
            {
                tb.hist.push_back(line);
                cnt++;
            }
        inf.close();

        if (tb.hist.size() > 10000)
            tb.hist.erase(tb.hist.begin(), tb.hist.begin() + (tb.hist.size() - 10000));
    }
    else
        cerr << "History file not found. Will create: " << hFile << "\n";
}

void MyTerm::svHist(const string &cmd)
{
    const char *home = getenv("HOME");
    if (!home)
    {
        cerr << "Warning: HOME environment variable not set. Cannot save history.\n";
        return;
    }

    hFile = string(home) + "/.myterm_history";

    ifstream checkFile(hFile);
    if (checkFile.is_open())
    {
        vector<string> lines;
        string line;
        while (getline(checkFile, line))
            lines.push_back(line);
        checkFile.close();

        if (lines.size() >= 10000)
        {
            ofstream trimFile(hFile, ios::trunc);
            for (size_t i = lines.size() - 9999; i < lines.size(); i++)
                trimFile << lines[i] << "\n";
            trimFile.close();
        }
    }

    ofstream outf(hFile, ios::app);
    if (!outf.is_open())
    {
        cerr << "Warning: Could not open history file for writing: " << hFile << "\n";
        return;
    }
    outf << cmd << "\n";
    outf.close();
}

string MyTerm::trim(const string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

string MyTerm::mkPmt(const Tab &tb)
{
    string p = tb.cDir;
    const char *home = getenv("HOME");
    if (home)
    {
        string shome(home);
        if (p.rfind(shome, 0) == 0)
            p = "~" + p.substr(shome.size());
    }
    return "CLLab@Terminal:" + p + "$ ";
}

void MyTerm::exCmd(const string &cmdRaw, Tab &tb)
{
    string cmd = trim(cmdRaw);
    if (cmd.empty())
        return;

    if (cmd == "clear")
    {
        tb.oBuf.clear();
        tb.sY = 0;
        return;
    }

    if (cmd == "exit" || cmd == "close")
    {
        exit(0);
    }

    if (cmd == "history")
    {
        size_t start = (tb.hist.size() > 1000) ? tb.hist.size() - 1000 : 0;
        for (size_t i = start; i < tb.hist.size(); i++)
            tb.oBuf.push_back(to_string(i + 1) + "  " + tb.hist[i]);
        return;
    }

    if (cmd == "cd" || cmd.rfind("cd ", 0) == 0)
    {
        string arg = trim(cmd.substr(2));
        if (arg.empty())
            arg = getenv("HOME") ? getenv("HOME") : "/";

        fs::path tgt = (arg == "..") ? fs::path(tb.cDir).parent_path()
                                     : fs::path(tb.cDir) / arg;
        try
        {
            if (fs::exists(tgt) && fs::is_directory(tgt))
                tb.cDir = fs::absolute(tgt).string();
            else
                tb.oBuf.push_back("cd: no such directory: " + arg);
        }
        catch (const exception &e)
        {
            tb.oBuf.push_back(string("cd error: ") + e.what());
        }
        return;
    }

    string cmdPart = cmd;
    string procCmd = cmdPart;

    size_t pos = 0;
    while ((pos = procCmd.find("\\n", pos)) != string::npos)
    {
        procCmd.replace(pos, 2, "\n");
        pos += 1;
    }

    string inFile, outFile;
    size_t inPos = cmd.find('<');
    size_t outPos = cmd.find('>');

    if (inPos != string::npos)
    {
        inFile = trim(cmd.substr(inPos + 1));
        cmdPart = trim(cmd.substr(0, inPos));

        if ((outPos != string::npos) && outPos > inPos)
        {
            outFile = trim(cmd.substr(outPos + 1));
            inFile = trim(cmd.substr(inPos + 1, outPos - inPos - 1));
            cmdPart = trim(cmd.substr(0, inPos));
        }
    }
    else if (outPos != string::npos)
    {
        outFile = trim(cmd.substr(outPos + 1));
        cmdPart = trim(cmd.substr(0, outPos));
    }

    int pfd[2];
    if (pipe(pfd) == -1)
    {
        tb.oBuf.push_back("pipe failed");
        return;
    }

    pid_t pid = fork();
    fgPid = pid;

    if (pid == 0)
    {
        close(pfd[0]);

        if (!inFile.empty())
        {
            fs::path inPath = fs::path(tb.cDir) / inFile;
            int infd = open(inPath.c_str(), O_RDONLY);
            if (infd < 0)
            {
                perror(("open input: " + inPath.string()).c_str());
                _exit(1);
            }
            dup2(infd, STDIN_FILENO);
            close(infd);
        }

        if (!outFile.empty())
        {
            fs::path outPath = fs::path(tb.cDir) / outFile;
            int outfd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (outfd < 0)
            {
                perror(("open output: " + outPath.string()).c_str());
                _exit(1);
            }
            dup2(outfd, STDOUT_FILENO);
            dup2(outfd, STDERR_FILENO);
            close(outfd);
        }
        else
        {
            dup2(pfd[1], STDOUT_FILENO);
            dup2(pfd[1], STDERR_FILENO);
        }

        close(pfd[1]);
        chdir(tb.cDir.c_str());

        istringstream iss(cmdPart);
        vector<char *> args;
        string token;
        while (iss >> token)
            args.push_back(strdup(token.c_str()));
        args.push_back(nullptr);

        if (args.size() > 0)
        {
            string firstArg(args[0]);
            if (firstArg.rfind("./", 0) == 0)
            {
                fs::path absPath = fs::path(tb.cDir) / firstArg;
                free(args[0]);
                args[0] = strdup(absPath.c_str());
            }
        }

        execvp(args[0], args.data());
        perror("execvp failed");
        _exit(1);
    }
    else if (pid > 0)
    {
        close(pfd[1]);

        if (outFile.empty())
        {
            char buf[1024];
            ssize_t r;
            string out;
            while ((r = read(pfd[0], buf, sizeof(buf) - 1)) > 0)
            {
                buf[r] = '\0';
                out += buf;
            }
            close(pfd[0]);
            waitpid(pid, nullptr, 0);

            istringstream iss(out);
            string line;
            while (getline(iss, line))
                tb.oBuf.push_back(line);
        }
        else
        {
            close(pfd[0]);
            waitpid(pid, nullptr, 0);
        }

        int vLines = (wH - TAB_HEIGHT - BOTTOM_MARGIN) / LINE_HEIGHT;
        tb.sY = max(0, (int)tb.oBuf.size() - vLines);
        fgPid = -1;
    }
}

void MyTerm::exPipeCmd(const string &cmdRaw, Tab &tb)
{
    string cmd = trim(cmdRaw);
    vector<string> cmds;

    size_t pos = 0, found;
    while ((found = cmd.find('|', pos)) != string::npos)
    {
        cmds.push_back(trim(cmd.substr(pos, found - pos)));
        pos = found + 1;
    }
    cmds.push_back(trim(cmd.substr(pos)));

    if (cmds.size() < 2)
    {
        exCmd(cmd, tb);
        return;
    }

    int n = cmds.size();
    int pfds[n - 1][2];

    for (int i = 0; i < n - 1; i++)
    {
        if (pipe(pfds[i]) == -1)
        {
            tb.oBuf.push_back("pipe creation failed");
            return;
        }
    }

    int outPipe[2];
    if (pipe(outPipe) == -1)
    {
        tb.oBuf.push_back("output pipe creation failed");
        return;
    }

    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (i > 0)
                dup2(pfds[i - 1][0], STDIN_FILENO);

            if (i < n - 1)
            {
                dup2(pfds[i][1], STDOUT_FILENO);
                dup2(pfds[i][1], STDERR_FILENO);
            }
            else
            {
                dup2(outPipe[1], STDOUT_FILENO);
                dup2(outPipe[1], STDERR_FILENO);
            }

            for (int j = 0; j < n - 1; j++)
            {
                close(pfds[j][0]);
                close(pfds[j][1]);
            }
            close(outPipe[0]);
            close(outPipe[1]);

            chdir(tb.cDir.c_str());

            istringstream iss(cmds[i]);
            vector<char *> args;
            string token;
            while (iss >> token)
            {
                args.push_back(strdup(token.c_str()));
            }
            args.push_back(nullptr);

            execvp(args[0], args.data());
            perror("execvp");
            _exit(1);
        }
    }

    for (int i = 0; i < n - 1; i++)
    {
        close(pfds[i][0]);
        close(pfds[i][1]);
    }
    close(outPipe[1]);

    char buf[1024];
    ssize_t r;
    string out;
    while ((r = read(outPipe[0], buf, sizeof(buf) - 1)) > 0)
    {
        buf[r] = '\0';
        out += buf;
    }
    close(outPipe[0]);

    for (int i = 0; i < n; i++)
        wait(nullptr);

    istringstream iss(out);
    string line;
    while (getline(iss, line))
        tb.oBuf.push_back(line);
}

void MyTerm::exMultiWatch(const string &cmdRaw, Tab &tb)
{
    size_t start = cmdRaw.find('[');
    size_t end = cmdRaw.find(']');
    if (start == string::npos || end == string::npos)
    {
        tb.oBuf.push_back("multiWatch syntax: multiWatch [\"cmd1\", \"cmd2\"]");
        return;
    }

    string cmdsStr = cmdRaw.substr(start + 1, end - start - 1);
    vector<string> cmds;

    size_t pos = 0;
    while (pos < cmdsStr.length())
    {
        size_t q1 = cmdsStr.find('\"', pos);
        if (q1 == string::npos)
            break;
        size_t q2 = cmdsStr.find('\"', q1 + 1);
        if (q2 == string::npos)
            break;

        string cmd = cmdsStr.substr(q1 + 1, q2 - q1 - 1);
        cmd = trim(cmd);
        if (!cmd.empty())
            cmds.push_back(cmd);
        pos = q2 + 1;
    }

    if (cmds.empty())
    {
        tb.oBuf.push_back("No commands to watch");
        return;
    }

    mwActive = true;
    mwPids.clear();
    vector<string> tFiles;
    vector<int> fFds;

    for (size_t i = 0; i < cmds.size(); i++)
    {
        string tFile = ".temp." + to_string(getpid()) + "_" + to_string(i) + ".txt";
        tFiles.push_back(tFile);

        int fd = open(tFile.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd < 0)
        {
            tb.oBuf.push_back("Failed to create temp file");
            continue;
        }
        close(fd);

        pid_t pid = fork();
        if (pid == 0)
        {
            while (true)
            {
                pid_t exPid = fork();
                if (exPid == 0)
                {
                    int fd = open(tFile.c_str(), O_WRONLY | O_TRUNC);
                    if (fd < 0)
                        _exit(1);

                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);

                    chdir(tb.cDir.c_str());

                    istringstream cmdIss(cmds[i]);
                    vector<char *> args;
                    string token;
                    while (cmdIss >> token)
                    {
                        args.push_back(strdup(token.c_str()));
                    }
                    args.push_back(nullptr);

                    execvp(args[0], args.data());
                    perror("execvp");
                    _exit(1);
                }

                waitpid(exPid, nullptr, 0);
                sleep(2);
            }
        }
        else if (pid > 0)
            mwPids.push_back(pid);
    }

    tb.oBuf.push_back("multiWatch started (Ctrl+C to stop)");
    tb.oBuf.push_back("Monitoring " + to_string(cmds.size()) + " commands...");
    rdraw = true;
    draw();

    for (const auto &tf : tFiles)
    {
        int fd = open(tf.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd >= 0)
            fFds.push_back(fd);
    }

    fd_set rFds;
    struct timeval tOut;
    vector<string> lOuts(cmds.size(), "");

    while (mwActive)
    {
        FD_ZERO(&rFds);
        int maxFd = -1;

        for (int fd : fFds)
        {
            FD_SET(fd, &rFds);
            if (fd > maxFd)
                maxFd = fd;
        }

        tOut.tv_sec = 1;
        tOut.tv_usec = 0;

        int ret = select(maxFd + 1, &rFds, NULL, NULL, &tOut);

        if (ret > 0)
        {
            for (size_t i = 0; i < fFds.size(); i++)
            {
                char buf[2048];
                lseek(fFds[i], 0, SEEK_SET);
                ssize_t n = read(fFds[i], buf, sizeof(buf) - 1);
                if (n > 0)
                {
                    buf[n] = '\0';
                    string out(buf);

                    if (out != lOuts[i])
                    {
                        lOuts[i] = out;
                        time_t now = time(NULL);
                        string tStr = ctime(&now);
                        tStr.pop_back();

                        tb.oBuf.push_back("\"" + cmds[i] + "\", " + tStr);
                        tb.oBuf.push_back("----------------------------------------------------");

                        istringstream iss(out);
                        string line;
                        while (getline(iss, line))
                            tb.oBuf.push_back(line);

                        tb.oBuf.push_back("----------------------------------------------------");
                        rdraw = true;
                        draw();
                    }
                }
            }
        }

        while (XPending(disp))
        {
            XEvent ev;
            XNextEvent(disp, &ev);
            if (ev.type == KeyPress)
            {
                hndlKeyPr(ev);
                if (!mwActive)
                    break;
            }
        }
    }

    for (auto pid : mwPids)
    {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }
    for (int fd : fFds)
        close(fd);
    for (const auto &tf : tFiles)
        unlink(tf.c_str());
    tb.oBuf.push_back("multiWatch stopped!");
}

void MyTerm::hndlCtrlR(Tab &tb)
{
    tb.sMode = true;
    tb.sBuf.clear();
    tb.oBuf.push_back("(reverse-i-search): ");
    tb.cPos = 0;
    int vLines = (wH - TAB_HEIGHT - BOTTOM_MARGIN) / LINE_HEIGHT;
    tb.sY = max(0, (int)tb.oBuf.size() - vLines);
    rdraw = true;
}

string MyTerm::lngCmnPfx(const vector<string> &strs)
{
    if (strs.empty())
        return "";
    string pfx = strs[0];
    for (const auto &s : strs)
    {
        int i = 0;
        while (i < (int)pfx.length() && i < (int)s.length() && pfx[i] == s[i])
            i++;
        pfx = pfx.substr(0, i);
    }
    return pfx;
}

void MyTerm::hndlTabCmp(Tab &tb)
{
    if (tb.iBuf.empty())
        return;

    size_t lSpace = tb.iBuf.find_last_of(" \t");
    string pfx = (lSpace == string::npos) ? tb.iBuf : tb.iBuf.substr(lSpace + 1);

    if (pfx.empty())
        return;

    vector<string> mtchs;
    DIR *dir = opendir(tb.cDir.c_str());
    if (!dir)
    {
        tb.oBuf.push_back("Error: Cannot read directory");
        rdraw = true;
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        string name = entry->d_name;
        if (name.find(pfx) == 0 && name != "." && name != "..")
            mtchs.push_back(name);
    }
    closedir(dir);

    sort(mtchs.begin(), mtchs.end());

    if (mtchs.empty())
        return;
    else if (mtchs.size() == 1)
    {
        if (lSpace == string::npos)
            tb.iBuf = mtchs[0];
        else
            tb.iBuf = tb.iBuf.substr(0, lSpace + 1) + mtchs[0];
        tb.cPos = tb.iBuf.length();
    }
    else
    {
        string cmn = lngCmnPfx(mtchs);
        if (cmn.length() > pfx.length())
        {
            if (lSpace == string::npos)
                tb.iBuf = cmn;
            else
                tb.iBuf = tb.iBuf.substr(0, lSpace + 1) + cmn;
            tb.cPos = tb.iBuf.length();
        }
        else
        {
            tb.tabSelMode = true;
            tb.tabMatches = mtchs;
            tb.tabPrefix = pfx;
            tb.tabLastSpace = lSpace;

            tb.oBuf.push_back("Matching files:");
            for (size_t i = 0; i < mtchs.size(); i++)
                tb.oBuf.push_back(to_string(i + 1) + ". " + mtchs[i]);
            tb.oBuf.push_back("Enter number to select (or press Esc to cancel):");
        }
    }
    rdraw = true;
}

void MyTerm::hndlTabSelection(Tab &tb, char input)
{
    if (!tb.tabSelMode)
        return;

    if (input >= '1' && input <= '9')
    {
        int choice = input - '0';

        if (choice > 0 && choice <= (int)tb.tabMatches.size())
        {
            string selected = tb.tabMatches[choice - 1];

            if (tb.tabLastSpace == string::npos)
                tb.iBuf = selected;
            else
                tb.iBuf = tb.iBuf.substr(0, tb.tabLastSpace + 1) + selected;

            tb.cPos = tb.iBuf.length();

            tb.oBuf.push_back("Selected: " + selected);

            // Exit selection mode
            tb.tabSelMode = false;
            tb.tabMatches.clear();
            tb.tabPrefix.clear();
            tb.tabLastSpace = string::npos;

            rdraw = true;
        }
        else
        {
            tb.oBuf.push_back("Invalid selection. Please enter a number between 1 and " +
                              to_string(tb.tabMatches.size()));
            rdraw = true;
        }
    }
}

void MyTerm::run()
{
    while (true)
    {
        XEvent ev;
        XNextEvent(disp, &ev);

        if (ev.type == ConfigureNotify)
        {
            wW = ev.xconfigure.width;
            wH = ev.xconfigure.height;
            rdraw = true;
        }
        if (ev.type == Expose)
            rdraw = true;
        else if (ev.type == KeyPress)
            hndlKeyPr(ev);
        else if (ev.type == ButtonPress || ev.type == ButtonRelease)
            hndlMouse(ev);
        else if (ev.type == SelectionNotify)
            hndlSel(ev);

        if (rdraw)
            draw();
    }
}

void MyTerm::hndlKeyPr(XEvent &ev)
{
    char buf[256];
    KeySym ks;
    Status stat;
    int len = 0;

    if (xic)
        len = XmbLookupString(xic, &ev.xkey, buf, sizeof(buf) - 1, &ks, &stat);
    else
        len = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &ks, nullptr);

    if (len < 0)
        len = 0;
    buf[len] = '\0';

    Tab &tb = tabs[cTab];

    if (ks == XK_Escape)
    {
        if (tb.tabSelMode)
        {
            tb.tabSelMode = false;
            tb.tabMatches.clear();
            tb.tabPrefix.clear();
            tb.tabLastSpace = string::npos;
            tb.oBuf.push_back("Selection cancelled");
            rdraw = true;
        }
        return;
    }

    if (tb.tabSelMode && len == 1 && buf[0] >= '1' && buf[0] <= '9')
    {
        hndlTabSelection(tb, buf[0]);
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_a)
    {
        tb.cPos = 0;
        rdraw = true;
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_e)
    {
        tb.cPos = tb.iBuf.length();
        rdraw = true;
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_r)
    {
        hndlCtrlR(tb);
        return;
    }
    if ((ev.xkey.state & ControlMask) && ks == XK_Tab)
    {
        cTab = (cTab + 1) % tabs.size();
        rdraw = true;
        return;
    }

    if (ks == XK_Tab)
    {
        if (!tb.tabSelMode)
            hndlTabCmp(tb);
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_c)
    {
        if (fgPid > 0)
        {
            kill(fgPid, SIGINT);
            tb.oBuf.push_back("^C");
            fgPid = -1;
        }
        if (mwActive)
        {
            mwActive = false;
            tb.oBuf.push_back("Stopping multiwatch...");
        }

        if (tb.tabSelMode)
        {
            tb.tabSelMode = false;
            tb.tabMatches.clear();
            tb.tabPrefix.clear();
            tb.tabLastSpace = string::npos;
            tb.oBuf.push_back("Selection cancelled");
        }

        tb.iBuf.clear();
        tb.cPos = 0;
        rdraw = true;
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_z)
    {
        if (fgPid > 0)
        {
            kill(fgPid, SIGTSTP);
            tb.oBuf.push_back("[" + to_string(fgPid) + "]+ Stopped");
            fgPid = -1;
        }
        tb.iBuf.clear();
        tb.cPos = 0;
        rdraw = true;
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_t)
    {
        tabs.emplace_back();
        cTab = (int)tabs.size() - 1;
        ldHist(tabs[cTab]);
        rdraw = true;
        return;
    }

    if ((ev.xkey.state & ControlMask) && ks == XK_w)
    {
        if (tabs.size() > 1)
        {
            tabs.erase(tabs.begin() + cTab);
            if (cTab >= (int)tabs.size())
                cTab = (int)tabs.size() - 1;
            rdraw = true;
        }
        return;
    }

    if ((ev.xkey.state & ControlMask) && (ks >= XK_1 && ks <= XK_9))
    {
        int idx = (int)(ks - XK_1);
        if (idx < (int)tabs.size())
        {
            cTab = idx;
            rdraw = true;
        }
        return;
    }

    if (ks == XK_Up)
    {
        if (!tb.hist.empty())
        {
            if (tb.hIdx == -1)
                tb.hIdx = (int)tb.hist.size() - 1;
            else if (tb.hIdx > 0)
                tb.hIdx--;
            tb.iBuf = tb.hist[tb.hIdx];
            tb.cPos = tb.iBuf.length();
            rdraw = true;
        }
        return;
    }

    if (ks == XK_Down)
    {
        if (!tb.hist.empty() && tb.hIdx != -1)
        {
            if (tb.hIdx < (int)tb.hist.size() - 1)
            {
                tb.hIdx++;
                tb.iBuf = tb.hist[tb.hIdx];
            }
            else
            {
                tb.hIdx = -1;
                tb.iBuf.clear();
            }
            tb.cPos = tb.iBuf.length();
            rdraw = true;
        }
        return;
    }

    if (ks == XK_Return)
    {
        if (tb.tabSelMode)
        {
            tb.oBuf.push_back("Please enter a number to select a file, or press Esc to cancel");
            rdraw = true;
            return;
        }

        if (tb.sMode)
        {
            tb.sMode = false;
            string sTrm = tb.sBuf;

            if (sTrm.empty())
            {
                tb.oBuf.push_back("Search cancelled");
                tb.sBuf.clear();
                tb.cPos = 0;
                rdraw = true;
                return;
            }

            bool fnd = false;
            for (int i = tb.hist.size() - 1; i >= 0; i--)
            {
                if (tb.hist[i] == sTrm)
                {
                    tb.oBuf.push_back("Found: " + tb.hist[i]);
                    tb.iBuf = tb.hist[i];
                    tb.cPos = tb.iBuf.length();
                    fnd = true;
                    break;
                }
            }

            if (!fnd)
            {
                for (int i = tb.hist.size() - 1; i >= 0; i--)
                {
                    if (sTrm.length() >= 2 &&
                        tb.hist[i].find(sTrm) != string::npos)
                    {
                        tb.oBuf.push_back("Match: " + tb.hist[i]);
                        tb.iBuf = tb.hist[i];
                        tb.cPos = tb.iBuf.length();
                        fnd = true;
                        break;
                    }
                }
            }

            if (!fnd)
            {
                tb.oBuf.push_back("No match found in history");
                tb.iBuf.clear();
                tb.cPos = 0;
            }
            tb.sBuf.clear();
            int vLines = (wH - BOTTOM_MARGIN) / LINE_HEIGHT;
            tb.sY = max(0, (int)tb.oBuf.size() - vLines);
            rdraw = true;
            return;
        }

        if (!tb.iBuf.empty() && tb.iBuf.back() == '\\')
        {
            tb.iBuf.pop_back();
            tb.iBuf += '\n';
            tb.cPos = tb.iBuf.length();
            rdraw = true;
            return;
        }

        string inp = trim(tb.iBuf);
        if (!inp.empty())
        {
            tb.oBuf.push_back(mkPmt(tb) + inp);

            if (tb.hist.empty() || tb.hist.back() != inp)
            {
                tb.hist.push_back(inp);
                if (tb.hist.size() > 10000)
                    tb.hist.erase(tb.hist.begin());
                svHist(inp);
            }

            if (inp.find("|") != string::npos)
                exPipeCmd(inp, tb);
            else if (inp.find("multiWatch") == 0)
                exMultiWatch(inp, tb);
            else
                exCmd(inp, tb);

            tb.hIdx = -1;
            int visLns = (wH - TAB_HEIGHT - BOTTOM_MARGIN) / LINE_HEIGHT;
            tb.sY = max(0, (int)tb.oBuf.size() - visLns);
        }
        tb.iBuf.clear();
        tb.cPos = 0;
        rdraw = true;
        return;
    }

    if (ks == XK_Left)
    {
        if (tb.sMode)
        {
            if (tb.cPos > 0)
                tb.cPos--;
        }
        else
        {
            if (tb.cPos > 0)
                tb.cPos--;
        }
        rdraw = true;
        return;
    }

    if (ks == XK_Right)
    {
        string &buf = tb.sMode ? tb.sBuf : tb.iBuf;
        if (tb.cPos < (int)buf.length())
            tb.cPos++;
        rdraw = true;
        return;
    }

    if (ks == XK_BackSpace)
    {
        if (tb.sMode)
        {
            if (!tb.sBuf.empty() && tb.cPos > 0)
            {
                tb.sBuf.erase(tb.cPos - 1, 1);
                tb.cPos--;
                if (!tb.oBuf.empty())
                    tb.oBuf.back() = "(reverse search): " + tb.sBuf;
            }
        }
        else
        {
            if (!tb.iBuf.empty() && tb.cPos > 0)
            {
                tb.iBuf.erase(tb.cPos - 1, 1);
                tb.cPos--;
            }
        }
        rdraw = true;
        return;
    }

    if (len > 0)
    {
        if (tb.tabSelMode)
            return;

        string mbStr(buf, len);
        if (tb.sMode)
        {
            tb.sBuf.insert(tb.cPos, mbStr);
            tb.cPos += mbStr.length();
            if (!tb.oBuf.empty())
                tb.oBuf.back() = "(reverse search): " + tb.sBuf;
        }
        else
        {
            tb.iBuf.insert(tb.cPos, mbStr);
            tb.cPos += mbStr.length();
        }
        rdraw = true;
    }
}

void MyTerm::hndlSel(XEvent &ev)
{
    if (ev.type == SelectionNotify)
    {
        Tab &tb = tabs[cTab];
        Atom tgtPrp = XInternAtom(disp, "XSEL_DATA", False);

        if (ev.xselection.property != None)
        {
            Atom atType;
            int atFmt;
            unsigned long nItms, bytesAft;
            unsigned char *dta = nullptr;

            XGetWindowProperty(disp, wnd, tgtPrp, 0L, (~0L), False, AnyPropertyType, &atType, &atFmt, &nItms, &bytesAft, &dta);

            if (dta)
            {
                string pstTxt((char *)dta);
                tb.iBuf.insert(tb.cPos, pstTxt);
                tb.cPos += pstTxt.length();

                XFree(dta);
                XDeleteProperty(disp, wnd, tgtPrp);
                rdraw = true;
            }
        }
    }
}

void MyTerm::hndlMouse(XEvent &ev)
{
    if (ev.type == ButtonPress && ev.xbutton.button == Button1)
    {
        int mx = ev.xbutton.x;
        int my = ev.xbutton.y;
        if (my < TAB_HEIGHT)
        {
            int plusX = (int)tabs.size() * (TAB_WIDTH + TAB_SPACING);
            if (mx >= plusX && mx < plusX + TAB_WIDTH / 2)
            {
                tabs.emplace_back();
                cTab = (int)tabs.size() - 1;
                ldHist(tabs[cTab]);
                rdraw = true;
                return;
            }

            for (int i = 0; i < (int)tabs.size(); i++)
            {
                int tabX = i * (TAB_WIDTH + TAB_SPACING);
                int xBtnX = tabX + TAB_WIDTH - 20;
                if (mx >= xBtnX && mx <= xBtnX + 10 && my >= 10 && my <= 20)
                {
                    if (tabs.size() > 1)
                    {
                        tabs.erase(tabs.begin() + i);
                        if (cTab >= (int)tabs.size())
                            cTab = (int)tabs.size() - 1;
                        else if (cTab > i)
                            cTab--;
                        rdraw = true;
                    }
                    return;
                }
            }

            int idx = mx / (TAB_WIDTH + TAB_SPACING);
            if (idx >= 0 && idx < (int)tabs.size())
            {
                cTab = idx;
                rdraw = true;
            }
        }
    }
    if (ev.type == ButtonPress && ev.xbutton.button == Button4)
    {
        tabs[cTab].sY = max(0, tabs[cTab].sY - 3);
        rdraw = true;
    }

    if (ev.type == ButtonPress && ev.xbutton.button == Button5)
    {
        tabs[cTab].sY += 3;
        rdraw = true;
    }
}

void MyTerm::draw()
{
    XClearWindow(disp, wnd);

    for (int i = 0; i < (int)tabs.size(); ++i)
    {
        int x = i * (TAB_WIDTH + TAB_SPACING);
        if (i == cTab)
            XSetForeground(disp, gc, gCol.pixel);
        else
            XSetForeground(disp, gc, dCol.pixel);
        XFillRectangle(disp, wnd, gc, x, 0, TAB_WIDTH, TAB_HEIGHT);

        XSetForeground(disp, gc, wCol.pixel);
        XDrawRectangle(disp, wnd, gc, x, 0, TAB_WIDTH, TAB_HEIGHT);

        string lbl = "Tab";
        int lblX = x + (TAB_WIDTH - XTextWidth(fnt, lbl.c_str(), (int)lbl.size())) / 2;
        XDrawString(disp, wnd, gc, lblX, TAB_HEIGHT - 8,
                    lbl.c_str(), (int)lbl.size());

        if (tabs.size() > 1)
        {
            int xBtnX = x + TAB_WIDTH - 20;
            int xBtnY = 8;
            XSetForeground(disp, gc, rCol.pixel);
            XDrawLine(disp, wnd, gc, xBtnX, xBtnY, xBtnX + 10, xBtnY + 10);
            XDrawLine(disp, wnd, gc, xBtnX + 10, xBtnY, xBtnX, xBtnY + 10);
        }
    }

    int plusX = (int)tabs.size() * (TAB_WIDTH + TAB_SPACING);
    XSetForeground(disp, gc, gCol.pixel);
    XFillRectangle(disp, wnd, gc, plusX, 0, TAB_WIDTH / 2, TAB_HEIGHT);
    XSetForeground(disp, gc, wCol.pixel);
    XDrawRectangle(disp, wnd, gc, plusX, 0, TAB_WIDTH / 2, TAB_HEIGHT);
    string plusLbl = "+";
    int plusLblX = plusX + (TAB_WIDTH / 2 - XTextWidth(fnt, plusLbl.c_str(), 1)) / 2;
    XDrawString(disp, wnd, gc, plusLblX, TAB_HEIGHT - 8, "+", 1);

    Tab &tb = tabs[cTab];

    int stY = TAB_HEIGHT + LINE_HEIGHT;
    int btmRsv = 2 * LINE_HEIGHT;
    int maxOY = wH - btmRsv;
    int avlblH = maxOY - stY;
    int visLns = avlblH / LINE_HEIGHT;

    if ((int)tb.oBuf.size() > visLns)
        tb.sY = max(0, min(tb.sY, (int)tb.oBuf.size() - visLns));
    else
        tb.sY = 0;
    int y = stY;
    for (int i = tb.sY; i < (int)tb.oBuf.size(); ++i)
    {
        if (y + LINE_HEIGHT > maxOY)
            break;
        XSetForeground(disp, gc, wCol.pixel);

        const string &line = tb.oBuf[i];
        if (fntset)
        {
            XmbDrawString(disp, wnd, fntset, gc, LEFT_MARGIN, y,
                          line.c_str(), (int)line.length());
        }
        else if (fnt)
        {
            XDrawString(disp, wnd, gc, LEFT_MARGIN, y,
                        line.c_str(), (int)line.length());
        }
        y += LINE_HEIGHT;
    }

    int inpY = y + LINE_HEIGHT;
    if (inpY > wH - 2 * LINE_HEIGHT)
        inpY = wH - 2 * LINE_HEIGHT;

    string pmt;
    if (tb.tabSelMode)
    {
        pmt = "Select file [1-" + to_string(tb.tabMatches.size()) + "]: ";
    }
    else
    {
        pmt = mkPmt(tb);
    }

    string uNm = tb.tabSelMode ? "" : "CLLab@Terminal:";
    string pth = tb.tabSelMode ? pmt : pmt.substr(uNm.size());

    if (!tb.tabSelMode)
    {
        XSetForeground(disp, gc, bgCol.pixel);
        if (fntset)
            XmbDrawString(disp, wnd, fntset, gc, LEFT_MARGIN, inpY, uNm.c_str(), uNm.length());
        else
            XDrawString(disp, wnd, gc, LEFT_MARGIN, inpY, uNm.c_str(), uNm.length());
    }

    XSetForeground(disp, gc, pCol.pixel);
    int uNmW = (fntset && !tb.tabSelMode) ? XmbTextEscapement(fntset, uNm.c_str(), uNm.length())
                                          : (!tb.tabSelMode ? XTextWidth(fnt, uNm.c_str(), uNm.length()) : 0);
    if (fntset)
        XmbDrawString(disp, wnd, fntset, gc, LEFT_MARGIN + uNmW, inpY, pth.c_str(), pth.length());
    else
        XDrawString(disp, wnd, gc, LEFT_MARGIN + uNmW, inpY, pth.c_str(), pth.length());

    XSetForeground(disp, gc, wCol.pixel);
    int pmtW = (fntset) ? XmbTextEscapement(fntset, pmt.c_str(), pmt.length())
                        : XTextWidth(fnt, pmt.c_str(), pmt.length());

    string dspBuf = tb.sMode ? tb.sBuf : (tb.tabSelMode ? "" : tb.iBuf);
    if (fntset)
        XmbDrawString(disp, wnd, fntset, gc, LEFT_MARGIN + pmtW, inpY, dspBuf.c_str(), dspBuf.length());
    else
        XDrawString(disp, wnd, gc, LEFT_MARGIN + pmtW, inpY, dspBuf.c_str(), dspBuf.length());

    int cPosW = (fntset) ? XmbTextEscapement(fntset, dspBuf.substr(0, tb.cPos).c_str(), tb.cPos)
                         : XTextWidth(fnt, dspBuf.substr(0, tb.cPos).c_str(), tb.cPos);
    int curX = LEFT_MARGIN + pmtW + cPosW;
    XSetForeground(disp, gc, bgCol.pixel);
    XFillRectangle(disp, wnd, gc, curX, inpY - 12, 2, 14);
    rdraw = false;
}

int main()
{
    MyTerm term;
    term.run();
    return 0;
}
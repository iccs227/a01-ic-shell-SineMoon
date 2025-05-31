/* ICCS227: Project 1: icsh
 * Name: Cherlyn Wijitthadarat
 * StudentID: 6480330
 * Tag: 0.7.0
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <map>
#include <termios.h>
#include <deque>

using namespace std;

// Job structure to track background jobs
struct Job {
    int jobId;
    pid_t pid;
    string command;
    bool isRunning;
    bool isStopped;
    bool isBackground;
};

pid_t fgJob = 0;
map<int, Job> jobs;           // Map to store active jobs
int nextJobId = 1;            // Counter for job IDs
pid_t shellPgid;              // Shell's process group ID

// History management
deque<string> commandHistory;
const size_t MAX_HISTORY_SIZE = 1000;
int historyIndex = -1;
string currentInput;

// Terminal state management
struct termios originalTermios;
bool rawModeEnabled = false;

// Enable raw mode for capturing arrow keys
void enableRawMode() {
    if (rawModeEnabled) return;
    
    tcgetattr(STDIN_FILENO, &originalTermios);
    struct termios raw = originalTermios;
    
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    rawModeEnabled = true;
}

// Disable raw mode
void disableRawMode() {
    if (!rawModeEnabled) return;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
    rawModeEnabled = false;
}

// Add command to history
void addToHistory(const string& command) {
    if (command.empty()) return;
    
    // No add duplicate consecutive 
    if (!commandHistory.empty() && commandHistory.back() == command) {
        return;
    }
    
    commandHistory.push_back(command);
   
    if (commandHistory.size() > MAX_HISTORY_SIZE) {
        commandHistory.pop_front();
    } 
    historyIndex = -1; // Reset history navigation
}

// Get command from history
string getHistoryCommand(int index) {
    if (index < 0 || index >= (int)commandHistory.size()) {
        return "";
    }
    return commandHistory[commandHistory.size() - 1 - index];
}

// Clear current line and rewrite with new content
void updateInputLine(const string& input, const string& prompt) {
    cout << "\r"; // Move cursor begin line
    cout << "\033[K"; // Clear entire line
    cout << prompt << input; // Print prompt & current
    cout.flush();
}

// Read input with history navigation support
string readInputWithHistory(const string& prompt) {
    cout << prompt;
    cout.flush();
    
    string input;
    historyIndex = -1;
    currentInput.clear();
    
    enableRawMode();
    
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n' || c == '\r') {
            cout << endl; // Enter pressed
            break;
        } else if (c == 127 || c == 8) {
            // Backspace
            if (!input.empty()) {
                input.pop_back();
                updateInputLine(input, prompt);
            }
        } else if (c == 27) {
            // Escape sequence - check for arrow keys
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A') {
                        // Up arrow
                        if (historyIndex == -1) {
                            // First time navigating - save current
                            currentInput = input;
                            historyIndex = 0;
                        } else {
                            historyIndex++;
                        }
                        
                        string historyCmd = getHistoryCommand(historyIndex);
                        if (!historyCmd.empty()) {
                            input = historyCmd;
                            updateInputLine(input, prompt);
                        } else {
                            // No more history
                            historyIndex--;
                        }
                    } else if (seq[1] == 'B') {
                        // Down arrow
                        if (historyIndex > 0) {
                            historyIndex--;
                            string historyCmd = getHistoryCommand(historyIndex);
                            input = historyCmd;
                            updateInputLine(input, prompt);
                        } else if (historyIndex == 0) {
                            // Return to original input
                            historyIndex = -1;
                            input = currentInput;
                            updateInputLine(input, prompt);
                        }
                    }
                }
            }
        } else if (c >= 32 && c <= 126) {
            // Printable character
            input += c;
            cout << c;
            cout.flush();
        }
        // Ignore other control characters
    }
    
    disableRawMode();
    return input;
}

vector<string> toTokens(const string& buffer) {
    vector<string> tokens;
    stringstream ss(buffer);
    string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void printToken(const vector<string>& tokens, size_t start) {
    for (size_t i = start; i < tokens.size(); ++i) {
        cout << tokens[i] << " ";
    }
    cout << endl;
}

// Function to reconstruct command string for job display
string reconstructCommand(const vector<string>& tokens) {
    string cmd;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) cmd += " ";
        cmd += tokens[i];
    }
    return cmd;
}

// Check for completed background jobs (non-blocking)
void checkCompletedJobs() {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto& pair : jobs) {
            Job& job = pair.second;
            if (job.pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    cout << "\n[" << job.jobId << "]+  Done                    " << job.command << endl;
                    jobs.erase(pair.first);
                } else if (WIFSTOPPED(status)) {
                    job.isStopped = true;
                    job.isRunning = false;
                    if (job.pid == fgJob) {
                        cout << "\n[" << job.jobId << "]+  Stopped                 " << job.command << endl;
                        fgJob = 0;
                        tcsetpgrp(STDIN_FILENO, shellPgid);
                    }
                } else if (WIFCONTINUED(status)) {
                    job.isStopped = false;
                    job.isRunning = true;
                }
                break;
            }
        }
    }
}

void handleSignal(int sig) {
    if (sig == SIGCHLD) {
        checkCompletedJobs();
    } else if (fgJob > 0) {
        if (sig == SIGINT) {
            kill(-fgJob, SIGINT);
            cout << "\nProcess " << fgJob << " was killed by SIGINT" << endl;
        } else if (sig == SIGTSTP) {
            kill(-fgJob, SIGTSTP);
            cout << "\nProcess " << fgJob << " was stopped by SIGTSTP" << endl;
        }
    }
}

void jobsCommand() {
    if (jobs.empty()) return;

    int mostRecent = 0;
    int secondMostRecent = 0;

    for (const auto& pair : jobs) {
        if (pair.first > mostRecent) {
            secondMostRecent = mostRecent;
            mostRecent = pair.first;
        } else if (pair.first > secondMostRecent) {
            secondMostRecent = pair.first;
        }
    }

    for (const auto& pair : jobs) {
        const Job& job = pair.second;
        char marker = ' ';
        if (job.jobId == mostRecent) marker = '+';
        else if (job.jobId == secondMostRecent) marker = '-';

        string status;
        if (job.isStopped) status = "Stopped";
        else if (job.isRunning) status = "Running";
        else status = "Done";

        cout << "[" << job.jobId << "]" << marker << "  " << status
             << "                 " << job.command;
        if (job.isBackground && !job.isStopped) cout << " &";
        cout << endl;
    }
}

void fgCommand(int jobId) {
    auto it = jobs.find(jobId);
    if (it == jobs.end()) {
        cout << "icsh: fg: %" << jobId << ": no such job" << endl;
        return;
    }

    Job& job = it->second;
    cout << job.command << endl;

    tcsetpgrp(STDIN_FILENO, job.pid);

    if (job.isStopped) {
        kill(-job.pid, SIGCONT);
        job.isStopped = false;
        job.isRunning = true;
    }

    job.isBackground = false;
    fgJob = job.pid;

    int status;
    waitpid(job.pid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, shellPgid);
    fgJob = 0;

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        jobs.erase(it);
    } else if (WIFSTOPPED(status)) {
        job.isStopped = true;
        job.isRunning = false;
        cout << "\n[" << job.jobId << "]+  Stopped                 " << job.command << endl;
    }
}

void bgCommand(int jobId) {
    auto it = jobs.find(jobId);
    if (it == jobs.end()) {
        cout << "icsh: bg: %" << jobId << ": no such job" << endl;
        return;
    }

    Job& job = it->second;

    if (!job.isStopped) {
        cout << "icsh: bg: job " << jobId << " already in background" << endl;
        return;
    }

    kill(-job.pid, SIGCONT);
    job.isStopped = false;
    job.isRunning = true;
    job.isBackground = true;

    cout << "[" << job.jobId << "]+ " << job.command << " &" << endl;
}

vector<string> IORedir(const vector<string>& curr, int& inFd, int& outFd) {
    vector<string> cmdWithoutRedir;

    for (size_t i = 0; i < curr.size(); ++i) {
        if (curr[i] == "<") {
            if (i + 1 >= curr.size() || curr[i + 1].empty()) {
                cerr << "icsh: no input file specified" << endl;
                return {};
            }
            inFd = open(curr[i + 1].c_str(), O_RDONLY);

            if (inFd == -1) {
                perror("Input file error");
                return {};
            }
            i++;
        } else if (curr[i] == ">") {
            if (i + 1 >= curr.size() || curr[i + 1].empty()) {
                cerr << "icsh: no output file specified" << endl;
                return {};
            }
            outFd = open(curr[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (outFd == -1) {
                perror("Output file error");
                return {};
            }
            i++;
        } else if (curr[i] != "&") { // Skip &
            cmdWithoutRedir.push_back(curr[i]);
        }
    }

    return cmdWithoutRedir;
}

int externalCommand(vector<string>& curr) {
    bool isBackground = false;
    if (!curr.empty() && curr.back() == "&") {
        isBackground = true;
        curr.pop_back();
    }

    int inFd = -1, outFd = -1;
    vector<string> cmdWithoutRedir = IORedir(curr, inFd, outFd);

    if (cmdWithoutRedir.empty() && !curr.empty()) return 1;

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        if (inFd != -1) close(inFd);
        if (outFd != -1) close(outFd);
        return 1;
    }

    if (pid == 0) {
        setpgid(0, 0);
        if (!isBackground) tcsetpgrp(STDIN_FILENO, getpid());

        if (inFd != -1) {
            if (dup2(inFd, STDIN_FILENO) == -1) { perror("dup2 for stdin"); exit(1); }
            close(inFd);
        }

        if (outFd != -1) {
            if (dup2(outFd, STDOUT_FILENO) == -1) { perror("dup2 for stdout"); exit(1); }
            close(outFd);
        }

        vector<char*> execArgs;
        for (auto& token : cmdWithoutRedir) execArgs.push_back(&token[0]);
        execArgs.push_back(nullptr);

        if (execvp(execArgs[0], execArgs.data()) == -1) {
            cout << "bad command" << endl;
            exit(1);
        }
    } else {
        if (inFd != -1) close(inFd);
        if (outFd != -1) close(outFd);

        setpgid(pid, pid);

        if (isBackground) {
            Job job{nextJobId++, pid, reconstructCommand(curr), true, false, true};
            jobs[job.jobId] = job;
            cout << "[" << job.jobId << "] " << pid << endl;
            return 0;
        } else {
            fgJob = pid;
            Job job{nextJobId++, pid, reconstructCommand(curr), true, false, false};
            jobs[job.jobId] = job;

            int status;
            waitpid(pid, &status, WUNTRACED);

            tcsetpgrp(STDIN_FILENO, shellPgid);
            fgJob = 0;

            if (WIFEXITED(status)) {
                jobs.erase(job.jobId);
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                jobs.erase(job.jobId);
                return 1;
            } else if (WIFSTOPPED(status)) {
                jobs[job.jobId].isStopped = true;
                jobs[job.jobId].isRunning = false;
                cout << "\n[" << job.jobId << "]+  Stopped                 " << job.command << endl;
                return 1;
            }
        }
    }
    return 1;
}


void command(vector<string>& curr, vector<string>& prev, int& lastExitStatus) {
    checkCompletedJobs();

    if (curr.empty()) return;

    if (curr[0] == "!!") {
        if (!prev.empty()) {
            if (curr.size() > 1) {
                vector<string> combinedCmd = prev;
                for (size_t i = 1; i < curr.size(); ++i) {
                    combinedCmd.push_back(curr[i]);
                }
                command(combinedCmd, prev, lastExitStatus);
            } else {
                command(prev, prev, lastExitStatus);
            }
        } else {
            cout << "No previous output" << endl;
        }
        return;
    }

    if (curr[0] == "jobs" && curr.size() == 1) {
        jobsCommand();
        lastExitStatus = 0;
        prev = curr;
        return;
    }

    if (curr[0] == "fg" && curr.size() == 2) {
        string jobSpec = curr[1];
        if (jobSpec[0] == '%') {
            int jobId = stoi(jobSpec.substr(1));
            fgCommand(jobId);
        } else {
            cout << "icsh: fg: usage: fg %job_id" << endl;
        }
        lastExitStatus = 0;
        prev = curr;
        return;
    }

    if (curr[0] == "bg" && curr.size() == 2) {
        string jobSpec = curr[1];
        if (jobSpec[0] == '%') {
            int jobId = stoi(jobSpec.substr(1));
            bgCommand(jobId);
        } else {
            cout << "icsh: bg: usage: bg %job_id" << endl;
        }
        lastExitStatus = 0;
        prev = curr;
        return;
    }

    if (curr[0] == "echo" && curr.size() == 2 && curr[1] == "$?") {
        cout << lastExitStatus << endl;
        lastExitStatus = 0;
    } else {
        if (curr[0] == "echo" && curr.size() > 1) {
            int inFd = -1;
            int outFd = -1;
            vector<string> cmdWithoutRedir = IORedir(curr, inFd, outFd);

            if (!cmdWithoutRedir.empty()) {
                int stdoutBackup = -1;
                if (outFd != -1) {
                    stdoutBackup = dup(STDOUT_FILENO);
                    dup2(outFd, STDOUT_FILENO);
                }

                for (size_t i = 1; i < cmdWithoutRedir.size(); ++i) {
                    cout << cmdWithoutRedir[i] << " ";
                }
                cout << endl;

                if (outFd != -1) {
                    dup2(stdoutBackup, STDOUT_FILENO);
                    close(stdoutBackup);
                    close(outFd);
                }

                lastExitStatus = 0;
            } else {
                lastExitStatus = 1;
            }
        } else if (curr[0] == "exit" && curr.size() == 2) {
            cout << "$ echo $?\n";
            int e = stoi(curr[1]);
            if (e > 255) e = e & 0xFF;
            cout << e << endl;
            exit(e);
        } else if (curr[0] == "exit" && curr.size() == 1) {
            cout << "$ echo $?\n0\n$" << endl;
            exit(0);
        } else {
            lastExitStatus = externalCommand(curr);
        }
        prev = curr;
    }
}

void readScript(const string& fileName) {
    ifstream file(fileName);
    if (!file.is_open()) return;

    string buffer;
    vector<string> prevBuffer;
    int lastExitStatus = 0;

    while (getline(file, buffer)) {
        if (buffer.empty()) continue;
        vector<string> currBuffer = toTokens(buffer);
        command(currBuffer, prevBuffer, lastExitStatus);
    }
}

void cleanup() {
    disableRawMode();
}

int main(int argc, char* argv[]) {
    // Register cleanup function
    atexit(cleanup);
    
    signal(SIGINT, handleSignal);
    signal(SIGTSTP, handleSignal);
    signal(SIGCHLD, handleSignal);
    signal(SIGTTOU, SIG_IGN);

    shellPgid = getpid();
    setpgid(shellPgid, shellPgid);
    tcsetpgrp(STDIN_FILENO, shellPgid);

    int lastExitStatus = 0;
    vector<string> prevBuffer;

    if (argc > 1) {
        readScript(argv[1]);
    } else {
        cout << "Starting IC shell (↑/↓ to navigate command history)" << endl;

        while (true) {
            string buffer = readInputWithHistory("icsh $ ");
            if (buffer.empty()) continue;
            
            addToHistory(buffer);
            
            vector<string> currBuffer = toTokens(buffer);
            command(currBuffer, prevBuffer, lastExitStatus);
        }
    }

    return 0;
}


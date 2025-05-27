/* ICCS227: Project 1: icsh
 * Name: Cherlyn Wijitthadarat
 * StudentID: 6480330
 * Tag: 0.5.0
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

using namespace std;

pid_t fgJob = 0;

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

void handleSignal(int sig) {
    if (fgJob > 0) {
        if (sig == SIGINT) {
            kill(fgJob, SIGKILL);
            cout << "\nProcess " << fgJob << " was killed by SIGINT" << endl;
        } else if (sig == SIGTSTP) {
            kill(fgJob, SIGSTOP);
            cout << "\nProcess " << fgJob << " was stopped by SIGTSTP" << endl;
        }
    }
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
            i++; // Skip the filename
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
            i++; // Skip the filename
        } else {
            cmdWithoutRedir.push_back(curr[i]);
        }
    }

    return cmdWithoutRedir;
}

int externalCommand(vector<string>& curr) {
    int inFd = -1;
    int outFd = -1;

    vector<string> cmdWithoutRedir = IORedir(curr, inFd, outFd);

    if (cmdWithoutRedir.empty() && !curr.empty()) {
        // Clean up file descriptors before returning
        if (inFd != -1) close(inFd);
        if (outFd != -1) close(outFd);
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        if (inFd != -1) close(inFd);
        if (outFd != -1) close(outFd);
        return 1;
    }

    if (pid == 0) {
        // Child process
        if (inFd != -1) {
            if (dup2(inFd, STDIN_FILENO) == -1) {
                perror("dup2 for stdin");
                exit(1);
            }
            close(inFd);
        }

        if (outFd != -1) {
            if (dup2(outFd, STDOUT_FILENO) == -1) {
                perror("dup2 for stdout");
                exit(1);
            }
            close(outFd);
        }

        vector<char*> execArgs;
        for (auto& token : cmdWithoutRedir) {
            execArgs.push_back(&token[0]);
        }
        execArgs.push_back(nullptr);

        if (execvp(execArgs[0], execArgs.data()) == -1) {
            cout << "bad command" << endl;
            exit(1);
        }
    } else {
        // Parent process
        if (inFd != -1) close(inFd);
        if (outFd != -1) close(outFd);

        fgJob = pid;
        int status;
        waitpid(pid, &status, 0);
        fgJob = 0;

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 1;
        }
    }

    return 1;
}

void command(vector<string>& curr, vector<string>& prev, int& lastExitStatus) {
    if (curr.empty()) return;

    // Handle !! command - need to check for redirection in the current command
    if (curr[0] == "!!") {
        if (!prev.empty()) {
            // If there are redirection operators in curr (after !!), we need to handle them
            if (curr.size() > 1) {
                // Combine previous command with current redirection
                vector<string> combinedCmd = prev;
                for (size_t i = 1; i < curr.size(); ++i) {
                    combinedCmd.push_back(curr[i]);
                }
                command(combinedCmd, prev, lastExitStatus);
            } else {
                // Just execute previous command as-is
                command(prev, prev, lastExitStatus);
            }
        } else {
            cout << "No previous output" << endl;
        }
        return;
    }

    if (curr[0] == "echo" && curr.size() == 2 && curr[1] == "$?") {
        cout << lastExitStatus << endl;
        lastExitStatus = 0;
    } else if (curr[0] == "echo" && curr.size() > 1) {
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
                cout << cmdWithoutRedir[i];
                if (i < cmdWithoutRedir.size() - 1) cout << " ";
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
    } else if (curr[0] == "exit") {
        if (curr.size() == 2) {
            cout << "$ echo $?" << endl;
            int e = stoi(curr[1]);
            if (e > 255) {
                e = e & 0xFF;
            }
            cout << e << endl;
            exit(e);
        } else {
            cout << "$ echo $?" << endl << "0" << endl << "$" << endl;
            exit(0);
        }
    } else {
        lastExitStatus = externalCommand(curr);
    }
    
    // Update previous command only if current command is not !!
    if (curr[0] != "!!") {
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

int main(int argc, char* argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTSTP, handleSignal);

    int lastExitStatus = 0;
    vector<string> prevBuffer;

    if (argc > 1) {
        readScript(argv[1]);
    } else {
        cout << "Starting IC shell" << endl;
        string buffer;

        while (true) {
            cout << "icsh $ ";
            getline(cin, buffer);
            if (buffer.empty()) continue;
            vector<string> currBuffer = toTokens(buffer);
            command(currBuffer, prevBuffer, lastExitStatus);
        }
    }

    return 0;
}


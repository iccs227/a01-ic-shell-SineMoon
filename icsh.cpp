/* ICCS227: Project 1: icsh
 * Name: Cherlyn Wijitthadarat
 * StudentID: 6480330
 * Tag: 0.3.0
 */

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <unistd.h>     
#include <sys/types.h>  
#include <sys/wait.h>

using namespace std;

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

void externalCommand(vector<string>& curr) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid == 0) { 
        vector<char*> execArgs;
        for (auto& token : curr) {
            execArgs.push_back(&token[0]);
        }
        execArgs.push_back(nullptr);

        if (execvp(execArgs[0], execArgs.data()) == -1) {
            cout << "bad command" << endl;
            exit(1);
        }
    } else {  
        waitpid(pid, nullptr, 0);
    }
}

void command(vector<string>& curr, vector<string>& prev) {
    if (curr.empty()) return;

    if (curr[0] == "!!" && curr.size() == 1) {
        if (!prev.empty()) {
            command(prev, prev);
        } else {
            cout << "No previous output" << endl;
        }

    } else {
        if (curr[0] == "echo" && curr.size() > 1) {
            printToken(curr, 1);
        } else if (curr[0] == "exit" && curr.size() == 2) {
            cout << "$ echo $?\n";
            int e = stoi(curr[1]);
            if (e > 255) {
                e = e & 0xFF;
            }
            cout << e << endl;
            exit(e);
        } else if (curr[0] == "exit" && curr.size() == 1) {
            cout << "$ echo $?\n0\n$" << endl;
            exit(0);
        } else {
            externalCommand(curr);
        }
        prev = curr;
    }
}


void readScript(const string& fileName) {
    ifstream file(fileName);
    if (!file.is_open()) {
        return;
    }

    string buffer;
    vector<string> prevBuffer;

    while (getline(file, buffer)) {
        if (buffer.empty()) continue;

        vector<string> currBuffer = toTokens(buffer);
        command(currBuffer, prevBuffer);

        prevBuffer = currBuffer;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        readScript(argv[1]);
    } else {
        cout << "Starting IC shell" << endl;
        string buffer;
        vector<string> prevBuffer;

        while (true) {
            cout << "icsh $ ";
            getline(cin, buffer);

            if (buffer.empty()) continue;

            vector<string> currBuffer = toTokens(buffer);
            command(currBuffer, prevBuffer);

            prevBuffer = currBuffer;
        }
    }
    return 0;
}


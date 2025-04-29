/* ICCS227: Project 1: icsh
 * Name: Cherlyn Wijitthadarat
 * StudentID: 6480330
 * Tag: 0.1.0
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>

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

void printTokens(const vector<string>& tokens, int start) {
    for (size_t i = start; i < tokens.size(); ++i) {
        cout << tokens[i] << " ";
    }
    cout << endl;
}

void command(const vector<string>& curr, const vector<string>& prev) {
    if (curr.empty()) return;

    if (curr[0] == "!!" && curr.size() == 1) {
        if (!prev.empty()) {
            for (const auto& tok : prev) {
                cout << tok << " ";
            }
            cout << endl;
	    command(prev, prev);
        } else {
            cout << "No previous output" << endl;
        }
    } else if (curr[0] == "echo" && curr.size() > 1) {
        printTokens(curr, 1);
    } else if (curr[0] == "exit" && curr.size() == 1) {
        cout << "bye" << endl;
        exit(0);
    } else if (curr[0] == "exit" && curr.size() == 2) {
        cout << "bye" << endl;
        int n = stoi(curr[1]);
        if (n > 255) {
            n = n & 0xFF;
        }
        exit(n);
    } else {
        cout << "bad command" << endl;
    }
}

int main() {
    cout << "Starting IC shell" << endl;

    vector<string> prevBuffer;
    string input;

    while (true) {
        cout << "icsh $ ";
        getline(cin, input);
        if (input.empty()) continue;

        vector<string> currBuffer = toTokens(input);
        command(currBuffer, prevBuffer);
        prevBuffer = currBuffer;
    }

    return 0;
}

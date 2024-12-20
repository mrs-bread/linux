#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <sstream>

using namespace std;

// Объявления функций
void saveHistory(const vector<string>& history);
void loadHistory(vector<string>& history);
void executeCommand(const string& command);
void echoCommand(const string& args);
void printEnvironmentVariable(const string& varName);
void executeBinary(const string& command);
void handleSIGHUP(int signum);
bool isBootDisk(const string& device);
void displayHistory(const vector<string>& history);
int main() {
    vector<string> history;
    loadHistory(history);

    signal(SIGHUP, handleSIGHUP);

    while (true) {
        string input;
        cout << "$ ";
        if (!getline(cin, input)) {
            cout << endl;
            break; // Выход по Ctrl+D
        }

        if (input == "\\q" || input == "exit") {
            break;
        }

        history.push_back(input);
        executeCommand(input);
    }

    saveHistory(history);
    return 0;
}

void displayHistory(const vector<string>& history) {
    for (size_t i = 0; i < history.size(); ++i) {
        cout << i + 1 << ": " << history[i] << endl;
    }
}

void saveHistory(const vector<string>& history) {
    ofstream file("shell_history.txt");
    for (const auto& cmd : history) {
        file << cmd << endl;
    }
}

void loadHistory(vector<string>& history) {
    ifstream file("shell_history.txt");
    string line;
    while (getline(file, line)) {
        history.push_back(line);
    }
}

void executeCommand(const string& command, const vector<string>& history) {
    if (command == "\\h") {
        displayHistory(history);
    } else if (command.substr(0, 5) == "echo ") {
        echoCommand(command.substr(5));
    } else if (command.substr(0, 3) == "\\e ") {
        printEnvironmentVariable(command.substr(3));
    } else if (command.substr(0, 3) == "\\l ") {
        string device = command.substr(3);
        if (isBootDisk(device)) {
            cout << device << " is a boot disk" << endl;
        } else if (command[0] == '/') {
        executeBinary(command);
    } else {
        // Выполнение базовых команд Linux
        pid_t pid = fork();
        if (pid == 0) {
            // Дочерний процесс
            execlp(command.c_str(), command.c_str(), NULL);
            cerr << "Command not found: " << command << endl;
            exit(1);
        } else if (pid > 0) {
            // Родительский процесс
            int status;
            waitpid(pid, &status, 0);
        } else {
            cerr << "Fork failed" << endl;
        }
    }
}

void echoCommand(const string& args) {
    cout << args << endl;
}

void printEnvironmentVariable(const string& varName) {
    char* value = getenv(varName.c_str());
    if (value) {
        cout << value << endl;
    } else {
        cout << "Environment variable not found: " << varName << endl;
    }
}

void executeBinary(const string& command) {
    vector<string> args;
    stringstream ss(command);
    string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }

    vector<char*> c_args;
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        execv(args[0].c_str(), c_args.data());
        cerr << "Failed to execute: " << args[0] << endl;
        exit(1);
    } else if (pid > 0) {
        // Родительский процесс
        int status;
        waitpid(pid, &status, 0);
    } else {
        cerr << "Fork failed" << endl;
    }
}

void handleSIGHUP(int signum) {
    cout << "Configuration reloaded" << endl;
}

bool isBootDisk(const string& device) {
    int fd = open(device.c_str(), O_RDONLY);
    if (fd == -1) {
        cerr << "Failed to open device: " << device << endl;
        return false;
    }

    unsigned char buffer[512];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    close(fd);

    if (bytesRead != sizeof(buffer)) {
        cerr << "Failed to read MBR from device: " << device << endl;
        return false;
    }

    // Проверяем сигнатуру 0x55AA в конце MBR
    return (buffer[510] == 0x55 && buffer[511] == 0xAA);
}

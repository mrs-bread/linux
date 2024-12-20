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

const string HISTORY_FILE = "history.txt";

// Объявления функций
void saveHistory(const vector<string>& history);
void loadHistory(vector<string>& history);
void executeCommand(const string& command, vector<string>& history);
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

        if (!input.empty()) {
            history.push_back(input);
            executeCommand(input, history);
            saveHistory(history);  // Сохраняем историю после каждой команды
        }
    }

    return 0;
}

void saveHistory(const vector<string>& history) {
    ofstream file(HISTORY_FILE, ios::app);  // Открываем файл в режиме добавления
    if (file.is_open()) {
        file << history.back() << endl;  // Записываем только последнюю команду
    }
}

void loadHistory(vector<string>& history) {
    ifstream file(HISTORY_FILE);
    string line;
    while (getline(file, line)) {
        history.push_back(line);
    }
}

void executeCommand(const string& command, vector<string>& history) {
    if (command.empty() || command.find_first_not_of(" \t") == string::npos) {
        cout << "Error: Empty command" << endl;
        return;
    }

    vector<string> args;
    istringstream iss(command);
    string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    if (args.empty()) {
        return;
    }

    if (args[0] == "\\h") {
        displayHistory(history);
    } else if (args[0] == "echo") {
        echoCommand(command.substr(5));
    } else if (args[0] == "\\e" && args.size() > 1) {
        printEnvironmentVariable(args[1]);
    } else if (args[0] == "\\l" && args.size() > 1) {
        if (isBootDisk(args[1])) {
            cout << args[1] << " is a boot disk" << endl;
        } else {
            cout << args[1] << " is not a boot disk" << endl;
        }
    } else if (args[0][0] == '/') {
        executeBinary(command);
    } else {
        // Выполнение базовых команд Linux
        pid_t pid = fork();
        if (pid == 0) {
            // Дочерний процесс
            vector<char*> c_args;
            for (const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);

            execvp(args[0].c_str(), c_args.data());
            // Если execvp вернулся, значит произошла ошибка
            cerr << "Error: Command not found or could not be executed: " << args[0] << endl;
            exit(1);
        } else if (pid > 0) {
            // Родительский процесс
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                cout << "Command exited with non-zero status: " << WEXITSTATUS(status) << endl;
            }
        } else {
            cerr << "Error: Fork failed" << endl;
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
    istringstream ss(command);
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

void displayHistory(const vector<string>& history) {
    for (size_t i = 0; i < history.size(); ++i) {
        cout << i + 1 << ": " << history[i] << endl;
    }
}

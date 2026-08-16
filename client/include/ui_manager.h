#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <string>

class UIManager {
public:
    static void printHeader();
    static void printPrompt();
    static void printHelp();
    static void printProgressBar(long long currentBytes, long long totalBytes);
    static void printResponse(const std::string& response);
};

#endif
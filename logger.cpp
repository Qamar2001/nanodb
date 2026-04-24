// logger.cpp
#include "logger.h"
#include <iostream>

Logger* g_logger = nullptr;

Logger::Logger() {
    fout.open("nanodb_execution.log", std::ios::out | std::ios::trunc);
    if (!fout.is_open()) {
        std::cerr << "[WARN] Could not open nanodb_execution.log\n";
    }
}

Logger::~Logger() {
    if (fout.is_open()) fout.close();
}

void Logger::log(const std::string& msg) {
    if (fout.is_open()) {
        fout << "[LOG] " << msg << "\n";
        fout.flush();
    }
}

void Logger::section(const std::string& title) {
    if (fout.is_open()) {
        fout << "\n===== " << title << " =====\n";
        fout.flush();
    }
}

void initLogger() {
    if (!g_logger) g_logger = new Logger();
}
void shutdownLogger() {
    if (g_logger) { delete g_logger; g_logger = nullptr; }
}

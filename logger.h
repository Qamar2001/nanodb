// logger.h - simple global logger that writes to nanodb_execution.log
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>

class Logger {
    std::ofstream fout;
public:
    Logger();
    ~Logger();
    void log(const std::string& msg);
    void section(const std::string& title);
};

// global logger instance (declared here, defined in logger.cpp)
extern Logger* g_logger;

void initLogger();
void shutdownLogger();

#endif

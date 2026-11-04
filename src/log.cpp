#include "gimbal_driver/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace gimbal::log {

namespace {

struct FileSink {
    std::string path;
    std::size_t max_bytes = 0;
    int backups = 0;
    std::ofstream stream;
};

std::mutex g_mutex;
FileSink g_file;

const char* levelName(Level level) {
    switch (level) {
        case Level::Info: return "INFO";
        case Level::Warning: return "WARNING";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ',' << std::setw(3) << std::setfill('0')
        << ms.count();
    return out.str();
}

void rotateIfNeeded() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (g_file.max_bytes == 0 || fs::file_size(g_file.path, ec) < g_file.max_bytes || ec) return;
    g_file.stream.close();
    for (int i = g_file.backups - 1; i >= 1; --i) {
        fs::rename(g_file.path + "." + std::to_string(i), g_file.path + "." + std::to_string(i + 1), ec);
    }
    if (g_file.backups > 0) fs::rename(g_file.path, g_file.path + ".1", ec);
    g_file.stream.open(g_file.path, std::ios::app);
}

}  // namespace

void setLogFile(const std::string& path, std::size_t max_bytes, int backups) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_file.path = path;
    g_file.max_bytes = max_bytes;
    g_file.backups = backups;
    g_file.stream.close();
    g_file.stream.open(path, std::ios::app);
}

void write(Level level, const std::string& name, const std::string& message) {
    const std::string line = timestamp() + " " + levelName(level) + " " + name + " " + message;
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cerr << line << '\n';
    if (g_file.stream.is_open()) {
        g_file.stream << line << '\n';
        g_file.stream.flush();
        rotateIfNeeded();
    }
}

}  // namespace gimbal::log

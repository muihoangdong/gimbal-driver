#pragma once

#include <cstddef>
#include <string>

namespace gimbal::log {

enum class Level { Info, Warning, Error };

// Ghi them ra file, xoay vong khi vuot max_bytes (giu backups ban cu).
void setLogFile(const std::string& path, std::size_t max_bytes = 5'000'000, int backups = 3);
void write(Level level, const std::string& name, const std::string& message);

inline void info(const std::string& name, const std::string& message) {
    write(Level::Info, name, message);
}
inline void warning(const std::string& name, const std::string& message) {
    write(Level::Warning, name, message);
}
inline void error(const std::string& name, const std::string& message) {
    write(Level::Error, name, message);
}

}  // namespace gimbal::log

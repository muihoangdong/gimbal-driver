#pragma once
// Tien ich dung chung cho cac cong cu kiem thu tren phan cung.

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace tools {

// Tham so dang --ten gia_tri hoac --co (khong co gia tri).
class Args {
public:
    Args(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key.rfind("--", 0) != 0) {
                bad_ = key;
                continue;
            }
            key = key.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                values_[key] = argv[++i];
            } else {
                values_[key] = "";
            }
        }
    }

    bool has(const std::string& key) const { return values_.count(key) > 0; }
    std::string get(const std::string& key, const std::string& fallback = "") const {
        auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }
    const std::string& bad() const { return bad_; }

private:
    std::map<std::string, std::string> values_;
    std::string bad_;
};

inline std::string now(bool millis = false) {
    const auto point = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(point);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (millis) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            point.time_since_epoch()) % 1000;
        out << '.' << std::setw(3) << std::setfill('0') << ms.count();
    }
    return out.str();
}

inline std::string csvField(const std::string& value) {
    if (value.find_first_of(",\"\n") == std::string::npos) return value;
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"') quoted += '"';
        quoted += c;
    }
    return quoted + "\"";
}

template <typename... Fields>
std::string csvRow(const Fields&... fields) {
    std::ostringstream out;
    bool first = true;
    ((out << (first ? "" : ",") << csvField(fields), first = false), ...);
    out << '\n';
    return out.str();
}

inline std::string prompt(const std::string& text) {
    std::cout << text << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

}  // namespace tools

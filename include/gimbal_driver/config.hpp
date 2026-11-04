#pragma once

#include <optional>
#include <string>

namespace gimbal {

struct Config {
    std::string driver = "mock";  // "mock" hoac "bridge"
    std::string bridge_url = "http://127.0.0.1:8787";
    std::optional<std::string> bridge_token;
    std::string host = "127.0.0.1";
    int port = 8080;
    std::optional<std::string> token;
    std::optional<std::string> log_file;
    double connect_timeout_s = 60.0;
    double monitor_interval_s = 1.0;
    int reconnect_attempts = 5;
};

// Nem std::invalid_argument khi file loi, co khoa la, hoac mo ra LAN ma khong co token.
Config loadConfig(const std::optional<std::string>& path);

}  // namespace gimbal

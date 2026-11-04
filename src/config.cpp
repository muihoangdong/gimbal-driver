#include "gimbal_driver/config.hpp"

#include <fstream>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace gimbal {

namespace {

using json = nlohmann::json;

std::optional<std::string> optionalString(const json& value) {
    if (value.is_null()) return std::nullopt;
    return value.get<std::string>();
}

}  // namespace

Config loadConfig(const std::optional<std::string>& path) {
    Config config;
    if (path) {
        std::ifstream file(*path);
        if (!file) throw std::invalid_argument("khong mo duoc " + *path);
        json data;
        try {
            data = json::parse(file);
        } catch (const json::parse_error& e) {
            throw std::invalid_argument(std::string("JSON loi: ") + e.what());
        }
        if (!data.is_object()) throw std::invalid_argument("cau hinh phai la mot object JSON");

        static const std::set<std::string> known = {
            "driver", "bridge_url", "bridge_token", "host", "port", "token", "log_file",
            "connect_timeout_s", "monitor_interval_s", "reconnect_attempts"};
        std::string unknown;
        for (const auto& item : data.items()) {
            if (!known.count(item.key())) unknown += (unknown.empty() ? "" : ", ") + item.key();
        }
        if (!unknown.empty()) throw std::invalid_argument("khoa cau hinh la: " + unknown);

        try {
            if (data.contains("driver")) config.driver = data["driver"].get<std::string>();
            if (data.contains("bridge_url")) config.bridge_url = data["bridge_url"].get<std::string>();
            if (data.contains("bridge_token")) config.bridge_token = optionalString(data["bridge_token"]);
            if (data.contains("host")) config.host = data["host"].get<std::string>();
            if (data.contains("port")) config.port = data["port"].get<int>();
            if (data.contains("token")) config.token = optionalString(data["token"]);
            if (data.contains("log_file")) config.log_file = optionalString(data["log_file"]);
            if (data.contains("connect_timeout_s")) {
                config.connect_timeout_s = data["connect_timeout_s"].get<double>();
            }
            if (data.contains("monitor_interval_s")) {
                config.monitor_interval_s = data["monitor_interval_s"].get<double>();
            }
            if (data.contains("reconnect_attempts")) {
                config.reconnect_attempts = data["reconnect_attempts"].get<int>();
            }
        } catch (const json::exception& e) {
            throw std::invalid_argument(std::string("sai kieu du lieu: ") + e.what());
        }
    }
    if (config.driver != "mock" && config.driver != "bridge") {
        throw std::invalid_argument("driver khong ho tro: " + config.driver);
    }
    const bool local = config.host == "127.0.0.1" || config.host == "localhost";
    if (!local && (!config.token || config.token->empty())) {
        throw std::invalid_argument("mo service ra mang thi phai dat token");
    }
    return config;
}

}  // namespace gimbal

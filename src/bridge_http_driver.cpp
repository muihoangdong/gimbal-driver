#include "gimbal_driver/bridge_http_driver.hpp"

#include <chrono>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gimbal {

namespace {

using json = nlohmann::json;

ErrorCode mapStatus(int status) {
    switch (status) {
        case 400: return ErrorCode::InvalidArgument;
        case 401: return ErrorCode::Auth;
        case 503: return ErrorCode::NotReady;
        default: return ErrorCode::Backend;
    }
}

void setTimeout(httplib::Client& client, double seconds) {
    const auto usec = std::chrono::microseconds(static_cast<long long>(seconds * 1e6));
    client.set_connection_timeout(usec);
    client.set_read_timeout(usec);
    client.set_write_timeout(usec);
}

json request(const BridgeHttpOptions& options, const std::string& method, const std::string& path,
             const json* body = nullptr, double timeout_s = 0.0) {
    httplib::Client client(options.base_url);
    setTimeout(client, timeout_s > 0.0 ? timeout_s : options.http_timeout_s);
    httplib::Headers headers;
    if (options.token) headers.emplace("X-Bridge-Token", *options.token);

    const std::string what = method + " " + path;
    httplib::Result result = method == "GET"
        ? client.Get(path, headers)
        : client.Post(path, headers, body ? body->dump() : "{}", "application/json");
    if (!result) {
        const auto err = result.error();
        if (err == httplib::Error::ConnectionTimeout || err == httplib::Error::Read) {
            throw GimbalError(ErrorCode::Timeout, what);
        }
        throw GimbalError(ErrorCode::Backend, what + ": " + httplib::to_string(err));
    }
    if (result->status >= 400) {
        throw GimbalError(mapStatus(result->status),
                          what + ": HTTP " + std::to_string(result->status));
    }
    if (result->body.empty()) return json();
    try {
        return json::parse(result->body);
    } catch (const json::parse_error&) {
        throw GimbalError(ErrorCode::Backend, what + ": JSON loi");
    }
}

// Bridge co the tra null hoac thieu truong khi gimbal vua rot; moi truong deu doc an toan.
const json& child(const json& object, const char* key) {
    static const json kEmpty = json::object();
    if (!object.is_object() || !object.contains(key) || !object[key].is_object()) return kEmpty;
    return object[key];
}

std::optional<double> optionalNumber(const json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object[key].is_number()) return std::nullopt;
    return object[key].get<double>();
}

bool boolOr(const json& object, const char* key, bool fallback) {
    if (!object.contains(key) || !object[key].is_boolean()) return fallback;
    return object[key].get<bool>();
}

std::string stringOr(const json& object, const char* key, const std::string& fallback) {
    if (!object.contains(key) || !object[key].is_string()) return fallback;
    return object[key].get<std::string>();
}

}  // namespace

BridgeHttpDriver::BridgeHttpDriver(BridgeHttpOptions options) : options_(std::move(options)) {
    while (!options_.base_url.empty() && options_.base_url.back() == '/') options_.base_url.pop_back();
}

BridgeHttpDriver::BridgeHttpDriver() : BridgeHttpDriver(BridgeHttpOptions()) {}

BridgeHttpDriver::~BridgeHttpDriver() { shutdownTimer(); }

Capabilities BridgeHttpDriver::getCapabilities() const {
    Capabilities caps;
    caps.telemetry = true;
    caps.angle_reference = "relative";
    return caps;
}

GimbalState BridgeHttpDriver::readGimbalState() {
    const json data = request(options_, "GET", "/state");
    const json& telemetry = child(data, "telemetry");
    GimbalState state;
    state.telemetry_stale = boolOr(telemetry, "stale", true);
    state.moving = boolOr(child(data, "move"), "active", false);
    if (!state.telemetry_stale) {
        state.pan_deg = optionalNumber(telemetry, "panDeg");
        state.tilt_deg = optionalNumber(telemetry, "tiltDeg");
        state.roll_deg = optionalNumber(telemetry, "rollDeg");
    }
    if (auto battery = optionalNumber(telemetry, "batteryPercent")) {
        state.battery_percent = static_cast<int>(*battery);
    }
    state.motor = stringOr(child(data, "motor"), "state", "UNKNOWN");
    if (auto rssi = optionalNumber(child(data, "ble"), "rssiDbm")) {
        state.link_rssi_dbm = static_cast<int>(*rssi);
    }
    return state;
}

ConnectionState BridgeHttpDriver::checkLink() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != ConnectionState::Connected) return state_;
    json label;
    try {
        label = request(options_, "GET", "/health/label");
    } catch (const GimbalError& e) {
        fail(e);
        return state_;
    }
    if (!(label.is_string() && label.get<std::string>() == "OK")) {
        fail(GimbalError(ErrorCode::NotConnected, "bridge: " + label.dump()));
    }
    return state_;
}

void BridgeHttpDriver::doOpen(double timeout_s) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::microseconds(static_cast<long long>(timeout_s * 1e6));
    std::string last = "chua co phan hoi";
    while (std::chrono::steady_clock::now() < deadline) {
        try {
            const json label = request(options_, "GET", "/health/label");
            if (label.is_string() && label.get<std::string>() == "OK") return;
            last = label.dump();
        } catch (const GimbalError& e) {
            last = e.what();
        }
        std::this_thread::sleep_for(
            std::chrono::microseconds(static_cast<long long>(options_.poll_s * 1e6)));
    }
    throw GimbalError(ErrorCode::Timeout, "bridge chua noi gimbal: " + last);
}

void BridgeHttpDriver::doClose() {}

void BridgeHttpDriver::doMove(int pan_dir, int tilt_dir) {
    const json body = pan_dir != 0 ? json{{"pan", pan_dir}} : json{{"tilt", tilt_dir}};
    request(options_, "POST", "/move/hold", &body);
}

void BridgeHttpDriver::doStop() { request(options_, "POST", "/move/stop"); }

void BridgeHttpDriver::doRecenter() {
    request(options_, "POST", "/center", nullptr, options_.center_timeout_s);
}

}  // namespace gimbal

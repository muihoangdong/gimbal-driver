#include "gimbal_driver/service.hpp"

#include <algorithm>
#include <exception>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "gimbal_driver/log.hpp"

namespace gimbal {

namespace {

using json = nlohmann::json;

int httpStatus(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument: return 400;
        case ErrorCode::Unsupported: return 501;
        case ErrorCode::NotConnected:
        case ErrorCode::NotReady:
        case ErrorCode::StopUnconfirmed: return 503;
        case ErrorCode::Timeout: return 504;
        default: return 502;
    }
}

template <typename T>
json optionalJson(const std::optional<T>& value) {
    return value ? json(*value) : json(nullptr);
}

std::string param(const httplib::Request& req, const char* name, const std::string& fallback) {
    return req.has_param(name) ? req.get_param_value(name) : fallback;
}

double parseNumber(const std::string& text, const char* name) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    throw GimbalError(ErrorCode::InvalidArgument, std::string(name) + " khong hop le");
}

int direction(const httplib::Request& req, const char* name) {
    const double value = parseNumber(param(req, name, "0"), name);
    return (value > 0) - (value < 0);
}

int durationMs(const httplib::Request& req) {
    const std::string text = param(req, "duration_ms", std::to_string(kMaxMoveDurationMs));
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    throw GimbalError(ErrorCode::InvalidArgument, "duration_ms khong hop le");
}

// So sanh khong ro ri thoi gian theo vi tri ky tu khac nhau.
bool constantTimeEquals(const std::string& a, const std::string& b) {
    unsigned char diff = static_cast<unsigned char>(a.size() != b.size());
    const std::size_t n = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        diff |= static_cast<unsigned char>(ca ^ cb);
    }
    return diff == 0;
}

json capabilitiesJson(const Capabilities& caps) {
    return {{"direction_control", caps.direction_control},
            {"speed_control", caps.speed_control},
            {"absolute_angle", caps.absolute_angle},
            {"simultaneous_axes", caps.simultaneous_axes},
            {"recenter", caps.recenter},
            {"telemetry", caps.telemetry},
            {"angle_reference", optionalJson(caps.angle_reference)},
            {"verified_on_hardware", caps.verified_on_hardware}};
}

json statusJson(GimbalDriver& driver) {
    const auto error = driver.getLastError();
    json status = {{"connection", toString(driver.getConnectionState())},
                   {"last_error", error ? json(toString(error->code())) : json(nullptr)}};
    try {
        const GimbalState s = driver.getGimbalState();
        status["gimbal"] = {{"moving", s.moving},
                            {"pan_deg", optionalJson(s.pan_deg)},
                            {"tilt_deg", optionalJson(s.tilt_deg)},
                            {"roll_deg", optionalJson(s.roll_deg)},
                            {"battery_percent", optionalJson(s.battery_percent)},
                            {"motor", s.motor},
                            {"telemetry_stale", s.telemetry_stale},
                            {"link_rssi_dbm", optionalJson(s.link_rssi_dbm)},
                            {"reference_set", s.reference_set}};
    } catch (const GimbalError& e) {
        status["gimbal"] = nullptr;
        status["gimbal_error"] = toString(e.code());
    }
    return status;
}

}  // namespace

struct GimbalService::Impl {
    GimbalDriver& driver;
    LinkMonitor* monitor;
    std::optional<std::string> token;
    httplib::Server server;

    Impl(GimbalDriver& d, LinkMonitor* m, std::optional<std::string> t)
        : driver(d), monitor(m), token(std::move(t)) {}

    bool authorized(const httplib::Request& req) const {
        if (!token || token->empty()) return true;
        std::string given = req.get_header_value("X-Gimbal-Token");
        if (given.empty()) given = param(req, "token", "");
        return constantTimeEquals(given, *token);
    }

    // Tra ve nullopt neu khong co route.
    std::optional<json> route(const httplib::Request& req) {
        const std::string& path = req.path;
        if (path == "/gimbal/move") {
            const int pan = direction(req, "pan");
            const int tilt = direction(req, "tilt");
            if (pan == 0 && tilt == 0) {
                driver.stop();
            } else {
                driver.move(pan, tilt, durationMs(req));
            }
            return json{{"gimbal", "move"}, {"pan", pan}, {"tilt", tilt}};
        }
        if (path == "/gimbal/stop") {
            driver.stop();
            return json{{"gimbal", "stop"}};
        }
        if (path == "/gimbal/recenter") {
            driver.recenter();
            return json{{"gimbal", "recenter"}};
        }
        if (path == "/gimbal/calibrate") {
            driver.setReference();
            return json{{"gimbal", "calibrate"}, {"reference_set", true}};
        }
        if (path == "/gimbal/calibrate/clear") {
            driver.clearReference();
            return json{{"gimbal", "calibrate"}, {"reference_set", false}};
        }
        if (path == "/gimbal/mode") {
            throw GimbalError(ErrorCode::Unsupported, "chua co lenh doi che do da kiem chung");
        }
        if (path == "/gimbal/status") return statusJson(driver);
        if (path == "/gimbal/capabilities") return capabilitiesJson(driver.getCapabilities());
        if (path == "/gimbal/connect") {
            if (monitor) monitor->reset();
            driver.connect(parseNumber(param(req, "timeout_s", "60"), "timeout_s"));
            return json{{"gimbal", "connected"}};
        }
        if (path == "/gimbal/disconnect") {
            driver.disconnect();
            return json{{"gimbal", "disconnected"}};
        }
        return std::nullopt;
    }

    void handle(const httplib::Request& req, httplib::Response& res) {
        auto reply = [&res](int status, const json& payload) {
            res.status = status;
            res.set_content(payload.dump(), "application/json");
        };
        if (!authorized(req)) {
            reply(401, {{"error", "AUTH"}});
            return;
        }
        try {
            const auto result = route(req);
            if (result) {
                reply(200, *result);
            } else {
                reply(404, {{"error", "NOT_FOUND"}});
            }
        } catch (const GimbalError& e) {
            reply(httpStatus(e.code()), {{"error", toString(e.code())}, {"message", e.what()}});
        } catch (const std::exception& e) {
            reply(500, {{"error", "INTERNAL"}, {"message", e.what()}});
        }
    }
};

GimbalService::GimbalService(GimbalDriver& driver, LinkMonitor* monitor,
                             std::optional<std::string> token)
    : impl_(std::make_unique<Impl>(driver, monitor, std::move(token))) {
    auto handler = [this](const httplib::Request& req, httplib::Response& res) {
        impl_->handle(req, res);
    };
    // App Windows/Android hien goi bang GET; POST cung duoc chap nhan.
    impl_->server.Get(".*", handler);
    impl_->server.Post(".*", handler);
    impl_->server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        log::info("gimbal_driver.service",
                  req.remote_addr + " " + req.method + " " + req.path + " " + std::to_string(res.status));
    });
}

GimbalService::~GimbalService() { stop(); }

int GimbalService::bind(const std::string& host, int port) {
    if (port == 0) return impl_->server.bind_to_any_port(host);
    return impl_->server.bind_to_port(host, port) ? port : -1;
}

void GimbalService::listen() { impl_->server.listen_after_bind(); }

void GimbalService::stop() { impl_->server.stop(); }

void GimbalService::waitUntilReady() { impl_->server.wait_until_ready(); }

}  // namespace gimbal

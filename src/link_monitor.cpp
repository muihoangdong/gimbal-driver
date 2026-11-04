#include "gimbal_driver/link_monitor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>

#include "gimbal_driver/log.hpp"

namespace gimbal {

LinkMonitor::LinkMonitor(GimbalDriver& driver, LinkMonitorOptions options, LinkEventCallback on_event)
    : driver_(driver), options_(options), on_event_(std::move(on_event)) {}

LinkMonitor::~LinkMonitor() { stop(); }

void LinkMonitor::start() {
    stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = false;
    }
    thread_ = std::thread([this] { run(); });
}

void LinkMonitor::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void LinkMonitor::reset() { gave_up_ = false; }

bool LinkMonitor::waitStop(double seconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, std::chrono::microseconds(static_cast<long long>(seconds * 1e6)),
                        [this] { return stop_requested_; });
}

void LinkMonitor::emit(const std::string& name, const std::string& detail) {
    log::warning("gimbal_driver.monitor", name + " " + detail);
    if (!on_event_) return;
    try {
        on_event_(name, detail);
    } catch (const std::exception& e) {
        log::error("gimbal_driver.monitor", std::string("on_event loi: ") + e.what());
    }
}

void LinkMonitor::run() {
    while (!waitStop(options_.interval_s)) {
        if (gave_up_ || driver_.checkLink() != ConnectionState::Fault) continue;
        const auto error = driver_.getLastError();
        emit("LINK_LOST", error ? error->what() : "");
        reconnect();
    }
}

void LinkMonitor::reconnect() {
    for (int attempt = 0; attempt < options_.max_attempts; ++attempt) {
        const double delay = std::min(options_.base_backoff_s * std::pow(2.0, attempt),
                                      options_.max_backoff_s);
        if (waitStop(delay)) return;
        try {
            driver_.connect(options_.connect_timeout_s);
        } catch (const GimbalError& e) {
            log::warning("gimbal_driver.monitor", "reconnect " + std::to_string(attempt + 1) + "/" +
                                                      std::to_string(options_.max_attempts) +
                                                      " that bai: " + e.what());
            continue;
        }
        emit("RECONNECTED", "lan " + std::to_string(attempt + 1));
        return;
    }
    gave_up_ = true;
    emit("GAVE_UP", "sau " + std::to_string(options_.max_attempts) + " lan");
}

}  // namespace gimbal

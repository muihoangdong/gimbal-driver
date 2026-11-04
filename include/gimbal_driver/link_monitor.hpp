#pragma once
// Giam sat lien ket va reconnect co gioi han/backoff; khong phu thuoc implementation.

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "gimbal_driver/gimbal_driver.hpp"

namespace gimbal {

struct LinkMonitorOptions {
    double interval_s = 1.0;
    int max_attempts = 5;
    double base_backoff_s = 2.0;
    double max_backoff_s = 30.0;
    double connect_timeout_s = 60.0;
};

// on_event(ten, chi_tiet) bao len Cinematic Controller: LINK_LOST, RECONNECTED, GAVE_UP.
using LinkEventCallback = std::function<void(const std::string&, const std::string&)>;

class LinkMonitor {
public:
    LinkMonitor(GimbalDriver& driver, LinkMonitorOptions options = LinkMonitorOptions(),
                LinkEventCallback on_event = nullptr);
    ~LinkMonitor();
    LinkMonitor(const LinkMonitor&) = delete;
    LinkMonitor& operator=(const LinkMonitor&) = delete;

    void start();
    void stop();
    // Cho phep thu lai sau GAVE_UP, vi du khi nguoi van hanh da bat lai gimbal.
    void reset();
    bool gaveUp() const { return gave_up_; }

private:
    void run();
    void reconnect();
    bool waitStop(double seconds);  // true neu duoc yeu cau dung
    void emit(const std::string& name, const std::string& detail);

    GimbalDriver& driver_;
    LinkMonitorOptions options_;
    LinkEventCallback on_event_;
    std::atomic<bool> gave_up_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
    std::thread thread_;
};

}  // namespace gimbal

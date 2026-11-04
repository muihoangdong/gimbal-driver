#pragma once
// HTTP service cho module khac goi; giu route /gimbal/* cua app Windows/Android.

#include <memory>
#include <optional>
#include <string>

#include "gimbal_driver/gimbal_driver.hpp"
#include "gimbal_driver/link_monitor.hpp"

namespace gimbal {

class GimbalService {
public:
    GimbalService(GimbalDriver& driver, LinkMonitor* monitor = nullptr,
                  std::optional<std::string> token = std::nullopt);
    ~GimbalService();
    GimbalService(const GimbalService&) = delete;
    GimbalService& operator=(const GimbalService&) = delete;

    // port = 0 chon cong trong; tra ve cong da bind, -1 neu that bai.
    int bind(const std::string& host, int port);
    void listen();  // chan den khi stop()
    void stop();
    void waitUntilReady();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gimbal

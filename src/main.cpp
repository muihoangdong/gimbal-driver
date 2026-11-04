// Chay service: gimbal-driver --config config.json

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "gimbal_driver/config.hpp"
#include "gimbal_driver/gimbal.hpp"
#include "gimbal_driver/log.hpp"
#include "gimbal_driver/service.hpp"

namespace {

std::atomic<bool> g_stop{false};

// Chi dat co trong signal handler; luong rieng moi goi service.stop().
void onSignal(int) { g_stop = true; }

std::unique_ptr<gimbal::GimbalDriver> buildDriver(const gimbal::Config& config) {
    if (config.driver == "bridge") {
        gimbal::BridgeHttpOptions options;
        options.base_url = config.bridge_url;
        options.token = config.bridge_token;
        return std::make_unique<gimbal::BridgeHttpDriver>(options);
    }
    return std::make_unique<gimbal::MockGimbalDriver>();
}

}  // namespace

int main(int argc, char** argv) {
    std::optional<std::string> config_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: gimbal-driver [--config config.json]\n";
            return 0;
        } else {
            std::cerr << "Tham so khong hop le: " << arg << '\n';
            return 2;
        }
    }

    gimbal::Config config;
    try {
        config = gimbal::loadConfig(config_path);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Loi cau hinh: " << e.what() << '\n';
        return 1;
    }
    if (config.log_file) gimbal::log::setLogFile(*config.log_file);

    auto driver = buildDriver(config);
    try {
        driver->connect(config.connect_timeout_s);
    } catch (const gimbal::GimbalError& e) {
        gimbal::log::error("gimbal_driver",
                           std::string("chua ket noi duoc, monitor se thu lai: ") + e.what());
    }

    gimbal::LinkMonitorOptions monitor_options;
    monitor_options.interval_s = config.monitor_interval_s;
    monitor_options.max_attempts = config.reconnect_attempts;
    monitor_options.connect_timeout_s = config.connect_timeout_s;
    gimbal::LinkMonitor monitor(*driver, monitor_options);
    monitor.start();

    gimbal::GimbalService service(*driver, &monitor, config.token);
    if (service.bind(config.host, config.port) < 0) {
        gimbal::log::error("gimbal_driver", "khong mo duoc cong " + std::to_string(config.port));
        monitor.stop();
        return 1;
    }
    gimbal::log::info("gimbal_driver", "service chay tai " + config.host + ":" +
                                           std::to_string(config.port) + " voi driver " +
                                           config.driver);

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::thread watcher([&service] {
        while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
        service.stop();
    });

    service.listen();
    g_stop = true;
    watcher.join();
    monitor.stop();
    driver->disconnect();
    return 0;
}

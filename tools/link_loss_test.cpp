// G-T05/G-T06: mat BLE khi gimbal dang quay, ghi thoi gian phat hien va noi lai.
//
// Nguoi van hanh tu gay mat BLE (che chan, dung bluetooth tren Pi) va dung sat de tat nguon gimbal.

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include "common.hpp"
#include "gimbal_driver/gimbal.hpp"

int main(int argc, char** argv) {
    tools::Args args(argc, argv);
    if (args.has("help") || !args.bad().empty()) {
        std::cout << "Usage: link_loss_test [--url URL] [--token T] [--wait-s 120]\n";
        return args.has("help") ? 0 : 2;
    }
    const double wait_s = std::stod(args.get("wait-s", "120"));
    gimbal::BridgeHttpOptions options;
    options.base_url = args.get("url", "http://127.0.0.1:8787");
    if (args.has("token")) options.token = args.get("token");
    gimbal::BridgeHttpDriver driver(options);

    using Clock = std::chrono::steady_clock;
    auto seconds_since = [](Clock::time_point t) {
        return std::chrono::duration<double>(Clock::now() - t).count();
    };
    std::cout << std::fixed << std::setprecision(1);

    driver.connect();
    tools::prompt("Bam Enter de quay; ngay sau do hay gay mat BLE...");
    driver.move(1, 0, 3000);
    const auto started = Clock::now();

    while (driver.checkLink() == gimbal::ConnectionState::Connected) {
        if (seconds_since(started) > wait_s) {
            std::cout << "Khong phat hien mat link; hanh dong ngat BLE chua co tac dung.\n";
            driver.disconnect();
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    const auto last = driver.getLastError();
    std::cout << "G-T05: phat hien sau " << seconds_since(started)
              << "s, loi: " << (last ? last->what() : "") << '\n';
    std::cout << "Quan sat: gimbal CO tiep tuc quay sau khi mat BLE khong? Ghi vao bao cao.\n";

    tools::prompt("Khoi phuc BLE roi bam Enter...");
    const auto recovered = Clock::now();
    try {
        driver.connect(wait_s);
    } catch (const gimbal::GimbalError& e) {
        std::cout << "G-T06: Fail, " << e.what() << '\n';
        return 1;
    }
    std::cout << "G-T06: noi lai sau " << seconds_since(recovered) << "s va da gui Stop\n";
    driver.disconnect();
    return 0;
}

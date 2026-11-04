// G-T08/G-T09: chay lien tuc, ghi thoi gian API va loi ra CSV.
//
// api_ms la thoi gian HTTP tra loi, KHONG phai do tre motor. Khi test G-T09, bat video/Wi-Fi
// song song va dat --label de so sanh giua cac lan chay.

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "common.hpp"
#include "gimbal_driver/gimbal.hpp"

namespace {

std::string fixed1(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

}  // namespace

int main(int argc, char** argv) {
    tools::Args args(argc, argv);
    if (args.has("help") || !args.bad().empty()) {
        std::cout << "Usage: soak_test [--mock] [--url URL] [--token T] [--minutes 30]\n"
                     "                 [--pulse-ms 300] [--pause-s 2] [--label baseline] [--out soak.csv]\n";
        return args.has("help") ? 0 : 2;
    }
    const double minutes = std::stod(args.get("minutes", "30"));
    const int pulse_ms = std::stoi(args.get("pulse-ms", "300"));
    const double pause_s = std::stod(args.get("pause-s", "2.0"));
    const std::string label = args.get("label", "baseline");
    const std::string out_path = args.get("out", "soak.csv");

    std::unique_ptr<gimbal::GimbalDriver> driver;
    if (args.has("mock")) {
        driver = std::make_unique<gimbal::MockGimbalDriver>();
    } else {
        gimbal::BridgeHttpOptions options;
        options.base_url = args.get("url", "http://127.0.0.1:8787");
        if (args.has("token")) options.token = args.get("token");
        driver = std::make_unique<gimbal::BridgeHttpDriver>(options);
    }
    driver->connect();

    std::vector<double> samples;
    int errors = 0;
    int link_losses = 0;
    int direction = 1;
    std::ofstream out(out_path, std::ios::app);
    out << tools::csvRow(std::string("time"), std::string("label"), std::string("command"),
                         std::string("api_ms"), std::string("error"));

    const auto end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(static_cast<long long>(minutes * 60000));
    while (std::chrono::steady_clock::now() < end) {
        // Dao chieu moi chu ky de gimbal khong troi dan ve mot phia.
        const auto started = std::chrono::steady_clock::now();
        std::string error;
        try {
            driver->move(direction, 0, pulse_ms);
        } catch (const gimbal::GimbalError& e) {
            error = gimbal::toString(e.code());
            ++errors;
        }
        const double api_ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - started).count();
        if (error.empty()) samples.push_back(api_ms);
        out << tools::csvRow(tools::now(true), label, std::string(direction > 0 ? "pan+1" : "pan-1"),
                             fixed1(api_ms), error);
        std::this_thread::sleep_for(std::chrono::milliseconds(pulse_ms + 200));
        direction = -direction;
        out.flush();

        if (driver->checkLink() != gimbal::ConnectionState::Connected) {
            ++link_losses;
            const auto last = driver->getLastError();
            out << tools::csvRow(tools::now(true), label, std::string("LINK_LOST"), std::string(),
                                 std::string(last ? last->what() : ""));
            try {
                driver->connect();
            } catch (const gimbal::GimbalError&) {
                ++errors;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(pause_s * 1000)));
    }
    driver->disconnect();

    std::cout << "Lenh OK: " << samples.size() << ", loi: " << errors
              << ", mat link: " << link_losses << '\n';
    if (!samples.empty()) {
        std::sort(samples.begin(), samples.end());
        const std::size_t n = samples.size();
        const double median = n % 2 ? samples[n / 2] : (samples[n / 2 - 1] + samples[n / 2]) / 2;
        const std::size_t p95 = std::max<long long>(0, static_cast<long long>(n * 0.95) - 1);
        std::cout << "api_ms trung vi " << fixed1(median) << ", p95 " << fixed1(samples[p95])
                  << ", max " << fixed1(samples.back()) << '\n';
    }
    return 0;
}

// Kiem thu ban tren RS 3 Mini: nguoi quan sat cham Pass/Fail, ket qua ghi ra CSV.
//
// Chay tren Pi sau khi rs3mini-bridge da chay. Gimbal phai duoc can bang va hanh trinh quay
// khong vuong vat gi; luon dung sat de tat nguon neu gimbal khong dung.

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

#include "common.hpp"
#include "gimbal_driver/gimbal.hpp"

namespace {

constexpr int kPulseMs = 500;

void runStep(std::ofstream& out, const std::string& test_id, const std::string& name,
             const std::function<void()>& action, const std::string& hint) {
    tools::prompt("\n[" + test_id + "] " + name + "\n  Quan sat: " + hint + "\n  Enter de chay...");
    const auto started = std::chrono::steady_clock::now();
    std::string error;
    try {
        action();
    } catch (const gimbal::GimbalError& e) {
        error = e.what();
        std::cout << "  LOI: " << error << '\n';
    }
    const auto api_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - started).count();
    const bool passed = tools::prompt("  Dat yeu cau? (y/n): ") == "y";
    const std::string note = tools::prompt("  Ghi chu: ");
    out << tools::csvRow(tools::now(), test_id, name, std::string(passed ? "Pass" : "Fail"),
                         std::to_string(api_ms), error, note);
    out.flush();
}

}  // namespace

int main(int argc, char** argv) {
    tools::Args args(argc, argv);
    if (args.has("help") || !args.has("firmware") || !args.bad().empty()) {
        std::cout << "Usage: bench_g01 --firmware <phien_ban> [--url URL] [--token T] [--out FILE]\n";
        return args.has("help") ? 0 : 2;
    }
    const std::string url = args.get("url", "http://127.0.0.1:8787");
    const std::string out_path = args.get("out", "bench_g01.csv");

    gimbal::BridgeHttpOptions options;
    options.base_url = url;
    if (args.has("token")) options.token = args.get("token");
    gimbal::BridgeHttpDriver driver(options);

    std::ofstream out(out_path, std::ios::app);
    out << tools::csvRow(std::string("time"), std::string("test_id"), std::string("name"),
                         std::string("result"), std::string("api_ms"), std::string("error"),
                         std::string("note"));
    out << tools::csvRow(tools::now(), std::string("ENV"), "firmware=" + args.get("firmware"),
                         std::string(), std::string(), std::string(), url);

    auto pulse = [&driver](int pan, int tilt) {
        return [&driver, pan, tilt] {
            driver.move(pan, tilt, kPulseMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(kPulseMs + 300));
        };
    };
    auto move_then_stop = [&driver] {
        driver.move(1, 0, 3000);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        driver.stop();
    };
    auto show_state = [&driver] {
        const gimbal::GimbalState s = driver.getGimbalState();
        auto fmt = [](const std::optional<double>& v) { return v ? std::to_string(*v) : "?"; };
        std::cout << "  pan=" << fmt(s.pan_deg) << " tilt=" << fmt(s.tilt_deg)
                  << " pin=" << (s.battery_percent ? std::to_string(*s.battery_percent) : "?")
                  << " motor=" << s.motor << " stale=" << s.telemetry_stale << '\n';
    };

    runStep(out, "G-T01", "Ket noi qua bridge", [&driver] { driver.connect(90); },
            "Bridge bao OK; api_ms la thoi gian cho ket noi");
    runStep(out, "G-T02a", "Pan trai", pulse(-1, 0), "Quay trai roi tu dung");
    runStep(out, "G-T02b", "Pan phai", pulse(1, 0), "Quay phai roi tu dung");
    runStep(out, "G-T03a", "Tilt len", pulse(0, 1), "Ngang len roi tu dung");
    runStep(out, "G-T03b", "Tilt xuong", pulse(0, -1), "Cui xuong roi tu dung");
    runStep(out, "G-T04a", "Stop giua chung", move_then_stop,
            "Dung sau khoang 0,5 giay, khong doi het 3 giay");
    runStep(out, "G-T04b", "Recenter", [&driver] { driver.recenter(); }, "Ve giua");
    runStep(out, "STATE", "Doc telemetry", show_state, "So voi thuc te; goc chi la tuong doi");

    std::cout << "\nDa ghi ket qua vao " << out_path
              << ". Bai G-T05/G-T06 dung link_loss_test.\n";
    return 0;
}

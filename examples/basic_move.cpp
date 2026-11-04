// Vi du dung API; them --bridge de chay voi rs3mini-bridge that.

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "gimbal_driver/gimbal.hpp"

int main(int argc, char** argv) {
    const bool use_bridge = argc > 1 && std::string(argv[1]) == "--bridge";
    std::unique_ptr<gimbal::GimbalDriver> gimbal;
    if (use_bridge) {
        gimbal = std::make_unique<gimbal::BridgeHttpDriver>();
    } else {
        gimbal = std::make_unique<gimbal::MockGimbalDriver>();
    }

    try {
        gimbal->connect();
        const gimbal::Capabilities caps = gimbal->getCapabilities();
        std::cout << "speed_control=" << caps.speed_control
                  << " absolute_angle=" << caps.absolute_angle
                  << " verified_on_hardware=" << caps.verified_on_hardware << '\n';

        gimbal->move(1, 0, 500);  // quay phai 0,5 giay
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        const gimbal::GimbalState state = gimbal->getGimbalState();
        std::cout << "moving=" << state.moving << " pan_deg="
                  << (state.pan_deg ? std::to_string(*state.pan_deg) : "?") << '\n';
        gimbal->recenter();
    } catch (const gimbal::GimbalError& e) {
        std::cout << "Loi " << gimbal::toString(e.code()) << ": " << e.what() << '\n';
    }
    gimbal->disconnect();
    return 0;
}

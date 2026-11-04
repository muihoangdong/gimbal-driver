#include "gimbal_driver/mock_driver.hpp"

namespace gimbal {

MockGimbalDriver::MockGimbalDriver(double rate_deg_s)
    : rate_deg_s_(rate_deg_s), last_update_(std::chrono::steady_clock::now()) {}

MockGimbalDriver::~MockGimbalDriver() { shutdownTimer(); }

Capabilities MockGimbalDriver::getCapabilities() const {
    Capabilities caps;
    caps.telemetry = true;
    caps.angle_reference = "relative";
    return caps;
}

GimbalState MockGimbalDriver::getGimbalState() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    integrate();
    GimbalState state;
    state.moving = physically_moving_;
    state.pan_deg = pan_deg_;
    state.tilt_deg = tilt_deg_;
    state.roll_deg = 0.0;
    state.battery_percent = 100;
    state.motor = "AWAKE";
    state.telemetry_stale = !link_up_;
    return state;
}

ConnectionState MockGimbalDriver::checkLink() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ == ConnectionState::Connected && !link_up_) {
        fail(GimbalError(ErrorCode::NotConnected, "mock: mat link"));
    }
    return state_;
}

void MockGimbalDriver::dropLink() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    link_up_ = false;
}

void MockGimbalDriver::restoreLink() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    link_up_ = true;
}

bool MockGimbalDriver::physicallyMoving() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return physically_moving_;
}

std::vector<std::string> MockGimbalDriver::commands() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return commands_;
}

void MockGimbalDriver::integrate() {
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_update_).count();
    last_update_ = now;
    if (physically_moving_) {
        pan_deg_ += dir_.first * rate_deg_s_ * dt;
        tilt_deg_ += dir_.second * rate_deg_s_ * dt;
    }
}

void MockGimbalDriver::requireLink() const {
    if (!link_up_) throw GimbalError(ErrorCode::NotConnected, "mock: mat link");
}

void MockGimbalDriver::doOpen(double) { requireLink(); }

void MockGimbalDriver::doClose() {}

void MockGimbalDriver::doMove(int pan_dir, int tilt_dir) {
    requireLink();
    integrate();
    dir_ = {pan_dir, tilt_dir};
    physically_moving_ = true;
    commands_.push_back("move " + std::to_string(pan_dir) + " " + std::to_string(tilt_dir));
}

void MockGimbalDriver::doStop() {
    requireLink();
    integrate();
    dir_ = {0, 0};
    physically_moving_ = false;
    commands_.push_back("stop");
}

void MockGimbalDriver::doRecenter() {
    requireLink();
    dir_ = {0, 0};
    physically_moving_ = false;
    pan_deg_ = 0.0;
    tilt_deg_ = 0.0;
    commands_.push_back("recenter");
}

}  // namespace gimbal

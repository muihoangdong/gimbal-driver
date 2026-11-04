#pragma once
// Gimbal gia lap de cac nhom tich hop khi chua co phan cung.

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "gimbal_driver/gimbal_driver.hpp"

namespace gimbal {

class MockGimbalDriver final : public GimbalDriver {
public:
    explicit MockGimbalDriver(double rate_deg_s = 30.0);
    ~MockGimbalDriver() override;

    Capabilities getCapabilities() const override;
    ConnectionState checkLink() override;

    // Gia lap mat Bluetooth; gimbal giu nguyen chuyen dong dang co.
    void dropLink();
    void restoreLink();
    bool physicallyMoving() const;
    std::vector<std::string> commands() const;

protected:
    GimbalState readGimbalState() override;
    void doOpen(double timeout_s) override;
    void doClose() override;
    void doMove(int pan_dir, int tilt_dir) override;
    void doStop() override;
    void doRecenter() override;

private:
    void integrate();
    void requireLink() const;

    double rate_deg_s_;
    double pan_deg_ = 0.0;
    double tilt_deg_ = 0.0;
    bool link_up_ = true;
    // Mo phong dong co: khi mat link, lenh Stop khong toi duoc gimbal.
    bool physically_moving_ = false;
    std::pair<int, int> dir_{0, 0};
    std::chrono::steady_clock::time_point last_update_;
    std::vector<std::string> commands_;
};

}  // namespace gimbal

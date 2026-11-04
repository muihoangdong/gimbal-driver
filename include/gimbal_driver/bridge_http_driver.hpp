#pragma once
// Adapter goi rs3mini-bridge qua HTTP; bridge giu ket noi BLE, khong phai driver nay.

#include <optional>
#include <string>

#include "gimbal_driver/gimbal_driver.hpp"

namespace gimbal {

struct BridgeHttpOptions {
    std::string base_url = "http://127.0.0.1:8787";
    std::optional<std::string> token;
    double http_timeout_s = 1.0;
    double center_timeout_s = 10.0;
    double poll_s = 0.5;
};

class BridgeHttpDriver final : public GimbalDriver {
public:
    explicit BridgeHttpDriver(BridgeHttpOptions options);
    BridgeHttpDriver();
    ~BridgeHttpDriver() override;

    // verified_on_hardware chi doi sau khi G01 chay tren RS 3 Mini cua nhom.
    Capabilities getCapabilities() const override;
    ConnectionState checkLink() override;

protected:
    GimbalState readGimbalState() override;
    void doOpen(double timeout_s) override;
    void doClose() override;  // BLE thuoc ve tien trinh bridge
    void doMove(int pan_dir, int tilt_dir) override;
    void doStop() override;
    void doRecenter() override;

private:
    BridgeHttpOptions options_;
};

}  // namespace gimbal

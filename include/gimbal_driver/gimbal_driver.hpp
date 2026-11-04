#pragma once
// Lop API ma Cinematic Controller goi; lop con chi cai dat do*().

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "gimbal_driver/types.hpp"

namespace gimbal {

class GimbalDriver {
public:
    virtual ~GimbalDriver();
    GimbalDriver(const GimbalDriver&) = delete;
    GimbalDriver& operator=(const GimbalDriver&) = delete;

    virtual Capabilities getCapabilities() const = 0;
    // Trang thai gimbal; neu da dat moc 0 thi goc tra ve la goc so voi moc.
    GimbalState getGimbalState();

    void connect(double timeout_s = 60.0);
    void disconnect();
    // Kiem tra lien ket that; duoc LinkMonitor goi dinh ky.
    virtual ConnectionState checkLink();
    ConnectionState getConnectionState() const;
    std::optional<GimbalError> getLastError() const;

    // Quay theo huong {-1, 0, 1} trong duration_ms; goi lai de gia han.
    void move(int pan_dir, int tilt_dir, int duration_ms);
    void stop();
    void recenter();

    // Lay huong hien tai lam moc 0 bang phan mem. Khong hieu chinh dong co gimbal
    // (Auto Tune / System Calibration van phai lam trong app Ronin).
    void setReference();
    void clearReference();

protected:
    GimbalDriver();

    virtual GimbalState readGimbalState() = 0;
    virtual void doOpen(double timeout_s) = 0;
    virtual void doClose() = 0;
    virtual void doMove(int pan_dir, int tilt_dir) = 0;
    virtual void doStop() = 0;
    virtual void doRecenter() = 0;

    void fail(const GimbalError& error);
    // Lop con final phai goi trong destructor, truoc khi thanh vien cua no bi huy.
    void shutdownTimer();

    mutable std::recursive_mutex mutex_;
    ConnectionState state_ = ConnectionState::Disconnected;

private:
    using Clock = std::chrono::steady_clock;

    void requireConnected() const;
    void armDeadline(int duration_ms);
    void cancelDeadline();
    void timerLoop();

    std::optional<GimbalError> last_error_;
    std::optional<GimbalState> reference_;
    bool moving_ = false;
    std::optional<std::pair<int, int>> last_move_;
    Clock::time_point last_send_{};

    std::mutex timer_mutex_;
    std::condition_variable timer_cv_;
    std::optional<Clock::time_point> deadline_;
    std::uint64_t generation_ = 0;
    bool timer_shutdown_ = false;
    std::thread timer_thread_;  // khai bao cuoi de khoi dong sau cac thanh vien khac
};

}  // namespace gimbal

#include "gimbal_driver/gimbal_driver.hpp"

#include "gimbal_driver/log.hpp"

#include <cmath>

namespace gimbal {

const char* toString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "DISCONNECTED";
        case ConnectionState::Connecting: return "CONNECTING";
        case ConnectionState::Connected: return "CONNECTED";
        case ConnectionState::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

const char* toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::InvalidArgument: return "INVALID_ARGUMENT";
        case ErrorCode::Unsupported: return "UNSUPPORTED";
        case ErrorCode::NotConnected: return "NOT_CONNECTED";
        case ErrorCode::NotReady: return "NOT_READY";
        case ErrorCode::Timeout: return "TIMEOUT";
        case ErrorCode::Auth: return "AUTH";
        case ErrorCode::Backend: return "BACKEND";
        case ErrorCode::StopUnconfirmed: return "STOP_UNCONFIRMED";
    }
    return "UNKNOWN";
}

namespace {
std::string formatError(ErrorCode code, const std::string& message) {
    return message.empty() ? toString(code) : std::string(toString(code)) + ": " + message;
}
}  // namespace

GimbalError::GimbalError(ErrorCode code, const std::string& message)
    : std::runtime_error(formatError(code, message)), code_(code) {}

GimbalDriver::GimbalDriver() : timer_thread_([this] { timerLoop(); }) {}

GimbalDriver::~GimbalDriver() { shutdownTimer(); }

void GimbalDriver::shutdownTimer() {
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        timer_shutdown_ = true;
    }
    timer_cv_.notify_all();
    if (!timer_thread_.joinable()) return;
    if (timer_thread_.get_id() == std::this_thread::get_id()) {
        timer_thread_.detach();
    } else {
        timer_thread_.join();
    }
}

void GimbalDriver::connect(double timeout_s) {
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        state_ = ConnectionState::Connecting;
    }
    // Khong giu khoa trong luc cho ket noi (co the toi timeout_s) de trang thai van tra loi ngay.
    try {
        doOpen(timeout_s);
    } catch (const GimbalError& e) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        fail(e);
        throw;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (state_ != ConnectionState::Connecting) {
        // disconnect() da duoc goi trong luc dang cho.
        throw GimbalError(ErrorCode::NotConnected, "ket noi bi huy");
    }
    state_ = ConnectionState::Connected;
    if (moving_) {
        // Noi lai sau khi mat link giua luc quay: dung truoc khi nhan lenh moi.
        try {
            doStop();
        } catch (const GimbalError& e) {
            GimbalError unconfirmed(ErrorCode::StopUnconfirmed, e.what());
            fail(unconfirmed);
            throw unconfirmed;
        }
        moving_ = false;
    }
    last_error_.reset();
    log::info("gimbal_driver", "connected");
}

void GimbalDriver::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (moving_) {
        try {
            stop();
        } catch (const GimbalError& e) {
            log::warning("gimbal_driver", std::string("stop truoc khi ngat that bai: ") + e.what());
        }
    }
    doClose();
    state_ = ConnectionState::Disconnected;
    log::info("gimbal_driver", "disconnected");
}

ConnectionState GimbalDriver::checkLink() { return getConnectionState(); }

ConnectionState GimbalDriver::getConnectionState() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return state_;
}

std::optional<GimbalError> GimbalDriver::getLastError() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return last_error_;
}

void GimbalDriver::move(int pan_dir, int tilt_dir, int duration_ms) {
    auto valid = [](int dir) { return dir >= -1 && dir <= 1; };
    if (!valid(pan_dir) || !valid(tilt_dir)) {
        throw GimbalError(ErrorCode::InvalidArgument, "huong phai la -1, 0 hoac 1");
    }
    if (pan_dir == 0 && tilt_dir == 0) {
        throw GimbalError(ErrorCode::InvalidArgument, "dung stop() de dung");
    }
    if (duration_ms <= 0 || duration_ms > kMaxMoveDurationMs) {
        throw GimbalError(ErrorCode::InvalidArgument,
                          "duration_ms phai trong (0, " + std::to_string(kMaxMoveDurationMs) + "]");
    }
    if (pan_dir != 0 && tilt_dir != 0 && !getCapabilities().simultaneous_axes) {
        throw GimbalError(ErrorCode::Unsupported, "khong ho tro quay 2 truc cung luc");
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    requireConnected();
    const auto now = Clock::now();
    const bool duplicate = moving_ && last_move_ == std::make_pair(pan_dir, tilt_dir) &&
                           now - last_send_ < std::chrono::milliseconds(kMoveResendMs);
    if (!duplicate) {
        try {
            doMove(pan_dir, tilt_dir);
        } catch (const GimbalError& e) {
            fail(e);
            throw;
        }
        last_move_ = std::make_pair(pan_dir, tilt_dir);
        last_send_ = now;
    }
    moving_ = true;
    armDeadline(duration_ms);
}

void GimbalDriver::stop() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    cancelDeadline();
    try {
        requireConnected();
        doStop();
    } catch (const GimbalError& e) {
        if (moving_) {
            GimbalError unconfirmed(ErrorCode::StopUnconfirmed, e.what());
            fail(unconfirmed);
            throw unconfirmed;
        }
        throw;
    }
    moving_ = false;
}

void GimbalDriver::recenter() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    requireConnected();
    cancelDeadline();
    moving_ = false;
    try {
        doRecenter();
    } catch (const GimbalError& e) {
        fail(e);
        throw;
    }
}

namespace {
// Hieu goc trong khoang [-180, 180) de qua moc +-180 khong nhay 360 do.
double angleDiff(double value, double reference) {
    double d = std::fmod(value - reference + 180.0, 360.0);
    if (d < 0) d += 360.0;
    return d - 180.0;
}

void subtract(std::optional<double>& value, const std::optional<double>& reference) {
    if (value && reference) value = angleDiff(*value, *reference);
}
}  // namespace

GimbalState GimbalDriver::getGimbalState() {
    GimbalState state = readGimbalState();
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (reference_) {
        subtract(state.pan_deg, reference_->pan_deg);
        subtract(state.tilt_deg, reference_->tilt_deg);
        subtract(state.roll_deg, reference_->roll_deg);
        state.reference_set = true;
    }
    return state;
}

void GimbalDriver::setReference() {
    const GimbalState raw = readGimbalState();
    if (raw.telemetry_stale || !raw.pan_deg || !raw.tilt_deg) {
        throw GimbalError(ErrorCode::NotReady, "chua co telemetry goc de dat moc");
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    reference_ = raw;
    log::info("gimbal_driver", "dat moc 0 tai pan=" + std::to_string(*raw.pan_deg) +
                                   " tilt=" + std::to_string(*raw.tilt_deg));
}

void GimbalDriver::clearReference() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    reference_.reset();
    log::info("gimbal_driver", "xoa moc 0");
}

void GimbalDriver::requireConnected() const {
    if (state_ != ConnectionState::Connected) {
        throw GimbalError(ErrorCode::NotConnected, toString(state_));
    }
}

void GimbalDriver::fail(const GimbalError& error) {
    last_error_ = error;
    // NOT_READY la tam thoi: lien ket van con, khong day sang FAULT.
    if (error.code() != ErrorCode::NotReady) {
        state_ = ConnectionState::Fault;
    }
    log::error("gimbal_driver", error.what());
}

void GimbalDriver::armDeadline(int duration_ms) {
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        ++generation_;
        deadline_ = Clock::now() + std::chrono::milliseconds(duration_ms);
    }
    timer_cv_.notify_all();
}

void GimbalDriver::cancelDeadline() {
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        ++generation_;
        deadline_.reset();
    }
    timer_cv_.notify_all();
}

void GimbalDriver::timerLoop() {
    std::unique_lock<std::mutex> timer_lock(timer_mutex_);
    while (!timer_shutdown_) {
        if (!deadline_) {
            timer_cv_.wait(timer_lock);
            continue;
        }
        const auto deadline = *deadline_;
        if (timer_cv_.wait_until(timer_lock, deadline) != std::cv_status::timeout) continue;
        if (timer_shutdown_ || !deadline_ || Clock::now() < *deadline_) continue;
        const std::uint64_t generation = generation_;
        timer_lock.unlock();
        {
            // Thu tu khoa mutex_ roi timer_mutex_ giong cac luong goi API.
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            bool current;
            {
                std::lock_guard<std::mutex> check(timer_mutex_);
                current = generation == generation_ && !timer_shutdown_;
            }
            if (current) {
                log::info("gimbal_driver", "het han lenh chuyen dong, tu dung");
                try {
                    stop();
                } catch (const GimbalError&) {
                    // da ghi vao last_error, trang thai FAULT
                }
            }
        }
        timer_lock.lock();
    }
}

}  // namespace gimbal

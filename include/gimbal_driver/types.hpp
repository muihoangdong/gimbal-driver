#pragma once
// Hop dong chung cua Gimbal Driver: khong chua logic tracking hay giao thuc BLE.

#include <optional>
#include <stdexcept>
#include <string>

namespace gimbal {

// Moi lenh chuyen dong phai co thoi han; driver tu dung khi het han.
constexpr int kMaxMoveDurationMs = 5000;
// Lenh cung huong goi lien tuc chi gia han noi bo, khong gui xuong gimbal moi lan.
constexpr int kMoveResendMs = 500;

enum class ConnectionState { Disconnected, Connecting, Connected, Fault };

enum class ErrorCode {
    InvalidArgument,
    Unsupported,
    NotConnected,
    NotReady,  // guard sau ket noi, motor sleep...
    Timeout,
    Auth,
    Backend,
    StopUnconfirmed,  // gimbal co the van dang quay
};

const char* toString(ConnectionState state);
const char* toString(ErrorCode code);

class GimbalError : public std::runtime_error {
public:
    explicit GimbalError(ErrorCode code, const std::string& message = "");
    ErrorCode code() const { return code_; }

private:
    ErrorCode code_;
};

// Chi khai bao dieu da duoc ho tro; mac dinh la khong ho tro.
struct Capabilities {
    bool direction_control = true;
    bool speed_control = false;
    bool absolute_angle = false;
    bool simultaneous_axes = false;
    bool recenter = true;
    bool telemetry = false;
    std::optional<std::string> angle_reference;  // "relative": khong phai huong da hieu chuan
    bool verified_on_hardware = false;
};

// Telemetry co the thieu: nullopt nghia la khong biet, khong phai bang 0.
struct GimbalState {
    bool moving = false;
    std::optional<double> pan_deg;
    std::optional<double> tilt_deg;
    std::optional<double> roll_deg;
    std::optional<int> battery_percent;
    std::string motor = "UNKNOWN";
    bool telemetry_stale = true;
    // Cuong do tin hieu lien ket toi gimbal (BLE), dung de theo doi do on dinh.
    std::optional<int> link_rssi_dbm;
    // true: goc da tru moc 0 do setReference() dat (moc phan mem, khong phai hieu chinh dong co).
    bool reference_set = false;
};

}  // namespace gimbal

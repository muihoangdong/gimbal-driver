# API Documentation

Hai cách dùng: gọi thư viện C++, hoặc gọi HTTP service. Cả hai cùng hành vi.

## Nguyên tắc

- Gimbal chỉ được điều khiển **theo hướng** (-1, 0, 1), **một trục mỗi lần**. Không có điều khiển tốc độ liên tục hay góc tuyệt đối.
- Mọi lệnh quay có thời hạn, tối đa 5000 ms. Hết hạn mà không gia hạn thì driver tự dừng.
- Góc đọc về là **tương đối**, không phải hướng đã hiệu chuẩn. `null` (HTTP) hoặc `std::nullopt` (C++) nghĩa là không biết.
- Luôn kiểm tra `getCapabilities()` trước khi dùng một tính năng.

## C++ API

```cpp
#include "gimbal_driver/gimbal.hpp"

gimbal::BridgeHttpOptions options;
options.base_url = "http://127.0.0.1:8787";
gimbal::BridgeHttpDriver driver(options);
gimbal::LinkMonitor monitor(driver, gimbal::LinkMonitorOptions(),
    [](const std::string& name, const std::string& detail) { std::cout << name << ' ' << detail << '\n'; });
driver.connect();
monitor.start();
```

Link với target CMake `gimbal_driver`. Lỗi được ném bằng `gimbal::GimbalError`, đọc mã qua `e.code()`.

| Hàm | Trả về | Lỗi có thể gặp |
|---|---|---|
| `connect(timeout_s = 60)` | — | `TIMEOUT`, `BACKEND`, `STOP_UNCONFIRMED` |
| `disconnect()` | — | — (dừng trước nếu đang quay) |
| `move(pan_dir, tilt_dir, duration_ms)` | — | `INVALID_ARGUMENT`, `UNSUPPORTED`, `NOT_CONNECTED`, `NOT_READY` |
| `stop()` | — | `STOP_UNCONFIRMED`, `NOT_CONNECTED` |
| `recenter()` | — | `NOT_CONNECTED`, `TIMEOUT` |
| `getConnectionState()` | `ConnectionState` | — |
| `getGimbalState()` | `GimbalState` | `BACKEND`, `TIMEOUT` |
| `getCapabilities()` | `Capabilities` | — |
| `getLastError()` | `std::optional<GimbalError>` | — |
| `checkLink()` | `ConnectionState` | — (dùng bởi `LinkMonitor`) |

**`move`:** gọi lại cùng hướng để gia hạn. Lệnh cùng hướng trong vòng 500 ms chỉ gia hạn nội bộ, không gửi lại xuống gimbal.

**`ConnectionState`:** `Disconnected` → `Connecting` → `Connected`; lỗi liên kết chuyển sang `Fault`. HTTP trả dạng chữ hoa (`CONNECTED`, `FAULT`...). Gọi `connect()` để thoát `FAULT`.

**`GimbalState`:** `moving`, `pan_deg`, `tilt_deg`, `roll_deg`, `battery_percent`, `motor` (`AWAKE`/`ASLEEP`/`UNKNOWN`), `telemetry_stale`. Trường góc và pin là `std::optional`.

**`Capabilities`:** `direction_control`, `speed_control`, `absolute_angle`, `simultaneous_axes`, `recenter`, `telemetry`, `angle_reference`, `verified_on_hardware`.

### LinkMonitor

Chạy luồng nền, mỗi `interval_s` kiểm tra liên kết. Khi mất liên kết: báo `LINK_LOST`, thử nối lại với backoff `2, 4, 8, 16, 30` giây, thành công báo `RECONNECTED`, hết lượt báo `GAVE_UP`. Gọi `reset()` để cho phép thử lại sau `GAVE_UP`.

## HTTP API

Chạy bằng `./build/gimbal-driver --config config.json`. Nhận cả GET và POST. Nếu cấu hình có `token`, gửi qua header `X-Gimbal-Token` hoặc tham số `?token=`.

| Route | Tham số | Kết quả 200 |
|---|---|---|
| `/gimbal/move` | `pan`, `tilt` (dấu của số), `duration_ms` (mặc định 5000) | `{"gimbal":"move","pan":1,"tilt":0}`; `pan=0&tilt=0` là dừng |
| `/gimbal/stop` | — | `{"gimbal":"stop"}` |
| `/gimbal/recenter` | — | `{"gimbal":"recenter"}` |
| `/gimbal/status` | — | `{"connection":"CONNECTED","last_error":null,"gimbal":{...}}` |
| `/gimbal/capabilities` | — | Các trường của `Capabilities` |
| `/gimbal/connect` | `timeout_s` | `{"gimbal":"connected"}`, đồng thời reset monitor |
| `/gimbal/disconnect` | — | `{"gimbal":"disconnected"}` |
| `/gimbal/mode` | — | Luôn trả 501, chưa có lệnh đã kiểm chứng |

Ví dụ:

```bash
curl -H "X-Gimbal-Token: <token>" "http://<IP_PI>:8080/gimbal/move?pan=1&tilt=0&duration_ms=500"
```

## Bảng mã lỗi

| Mã | HTTP | Ý nghĩa | Bên gọi nên làm |
|---|---|---|---|
| `INVALID_ARGUMENT` | 400 | Tham số sai | Sửa lời gọi |
| `AUTH` | 401 / 502 | Sai token service (401) hoặc token bridge (502) | Kiểm tra cấu hình |
| `UNSUPPORTED` | 501 | Thiết bị không hỗ trợ | Dùng `getCapabilities()` |
| `NOT_CONNECTED` | 503 | Chưa hoặc mất kết nối | Chờ monitor nối lại hoặc gọi `connect` |
| `NOT_READY` | 503 | Guard sau kết nối, motor sleep | Thử lại sau vài giây |
| `STOP_UNCONFIRMED` | 503 | Không xác nhận được lệnh dừng | **Gimbal có thể vẫn quay**; dừng tracking, cảnh báo người vận hành |
| `TIMEOUT` | 504 | Không phản hồi đúng hạn | Coi như chưa chắc lệnh đã thực thi |
| `BACKEND` | 502 | Bridge lỗi hoặc không truy cập được | Kiểm tra bridge |

## Lưu ý cho Cinematic Controller

- Visual Servoing nên gửi `move` mỗi 100–300 ms với `duration_ms` ngắn (ví dụ 300) để lệnh tự hết hạn nếu tracking treo.
- Khi nhận `STOP_UNCONFIRMED`, `LINK_LOST` hoặc `GAVE_UP`: ngừng tracking, không tự gửi lệnh mới.
- Không dùng `pan_deg`/`tilt_deg` làm vòng kín góc tuyệt đối; chỉ dùng xu hướng thay đổi.

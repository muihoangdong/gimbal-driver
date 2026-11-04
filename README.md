# Gimbal Driver

Driver độc lập để Cinematic Controller điều khiển DJI RS 3 Mini mà không phụ thuộc BLE. Viết bằng C++17, build bằng CMake.

**Trạng thái:** mới kiểm thử với Mock và bridge giả lập, chưa chạy trên gimbal thật.

## Tài liệu

| Sản phẩm bàn giao | File | Trạng thái |
|---|---|---|
| Mã nguồn Gimbal Driver | `include/`, `src/` | Xong, chưa thử trên phần cứng |
| File cấu hình và hướng dẫn cài đặt | `config.example.json`, [docs/INSTALL.md](docs/INSTALL.md) | Xong, chưa thử trên Pi |
| API Documentation | [docs/API.md](docs/API.md) | Xong |
| Demo điều khiển từ Raspberry Pi | `examples/basic_move.cpp`, `tools/bench_g01.cpp` | Chờ chạy trên Pi và quay video |
| Bộ test độc lập | `tests/`, `tools/` | Test tự động xong; bài test phần cứng chờ chạy |
| Báo cáo Lock/Follow | [docs/REPORT_LOCK_FOLLOW.md](docs/REPORT_LOCK_FOLLOW.md) | Khung, chờ số liệu |
| Báo cáo độ trễ và độ ổn định BLE | [docs/REPORT_LATENCY_BLE.md](docs/REPORT_LATENCY_BLE.md) | Khung, chờ số liệu |
| Danh sách giới hạn kỹ thuật | [docs/LIMITATIONS.md](docs/LIMITATIONS.md) | Bản đầu, cập nhật sau khi đo |

## Implementation

| Class | Dùng khi |
|---|---|
| `gimbal::MockGimbalDriver` | Tích hợp sớm, không cần phần cứng |
| `gimbal::BridgeHttpDriver` | Gọi [rs3mini-bridge](https://github.com/b-rutledge-code/rs3mini-bridge) trên Pi |

## Build

Cần CMake 3.20+, trình biên dịch C++17 (g++ trên Pi, MSVC trên Windows) và Internet ở lần build đầu. CMake chỉ tải 2 file header, khoảng 1,2 MB, có kiểm tra SHA-256: `httplib.h` (cpp-httplib v0.18.3) và `json.hpp` (nlohmann/json v3.11.3).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Muốn build cả bộ test thì thêm `-DGIMBAL_BUILD_TESTS=ON`; khi đó CMake tải thêm GoogleTest v1.15.2.

## Chép lên Raspberry Pi

Chỉ cần mã nguồn, khoảng 100 KB; không chép thư mục `build/` vì Pi sẽ tự build. Từ máy Windows, trong thư mục cha của `gimbal-driver` (thay `pi` và IP cho đúng):

```bash
tar --exclude=build --exclude=.git -czf gimbal-driver.tar.gz gimbal-driver
scp gimbal-driver.tar.gz pi@192.168.2.93:~
ssh pi@192.168.2.93 "tar -xzf gimbal-driver.tar.gz"
```

Hoặc clone trực tiếp trên Pi bằng `git clone --depth 1`.

## API

```cpp
#include "gimbal_driver/gimbal.hpp"

gimbal::MockGimbalDriver g;
g.connect();
g.move(/*pan_dir=*/1, /*tilt_dir=*/0, /*duration_ms=*/500);  // hết hạn sẽ tự dừng
g.stop();
g.recenter();
g.getConnectionState();
g.getGimbalState();
g.getCapabilities();
g.disconnect();
```

- `move` chỉ nhận hướng -1, 0, 1 và một trục mỗi lần. Tối đa 5000 ms; gọi lại để gia hạn.
- Giá trị góc là tương đối. `std::nullopt` nghĩa là không biết.
- Lỗi được báo bằng ngoại lệ `gimbal::GimbalError`, đọc mã qua `code()`.
- `LinkMonitor` giám sát kết nối và tự nối lại có backoff, giới hạn số lần. Service đã bật sẵn; nếu dùng thư viện trực tiếp thì tự khởi động nó.
- Lệnh `move` cùng hướng gọi dồn dập chỉ gia hạn, cứ 500 ms mới gửi xuống gimbal một lần.
- Sau khi nối lại, driver gửi Stop trước khi nhận lệnh mới nếu lệnh trước bị gián đoạn.
- `disconnect()` của `BridgeHttpDriver` không ngắt BLE, vì bridge giữ kết nối.

Chi tiết và bảng mã lỗi: [docs/API.md](docs/API.md).

## HTTP service

```bash
cp config.example.json config.json
./build/gimbal-driver --config config.json
```

Không truyền `--config` thì chạy Mock tại `127.0.0.1:8080`. Mở ra mạng LAN bắt buộc phải đặt `token`; gửi token qua header `X-Gimbal-Token` hoặc `?token=`.

| Route | Tác dụng |
|---|---|
| `/gimbal/move?pan=1&tilt=0&duration_ms=500` | Quay; `pan=0&tilt=0` là dừng |
| `/gimbal/stop`, `/gimbal/recenter` | Dừng, căn giữa |
| `/gimbal/status`, `/gimbal/capabilities` | Trạng thái, khả năng |
| `/gimbal/connect`, `/gimbal/disconnect` | Nối, ngắt và thử lại sau khi monitor bỏ cuộc |
| `/gimbal/mode` | Trả 501, chưa có lệnh đã kiểm chứng |

Thiếu `duration_ms` thì lấy 5000 ms. App Windows hiện chưa gửi token, cần sửa trước khi nối vào service có token. Nếu chạy bridge với `MOVE_HOLD_MS=2000`, lệnh giữ nên được gọi lại trong vòng 2 giây.

Mẫu systemd nằm ở `deploy/gimbal-driver.service`.

## Kiểm thử

```bash
cmake -S . -B build -DGIMBAL_BUILD_TESTS=ON && cmake --build build -j2
./build/gimbal_tests
```

Thử trên phần cứng: trên Pi, chạy `rs3mini-bridge` với `GIMBAL_NAME_RE` là hậu tố tên gimbal, rồi chạy:

```bash
./build/bench_g01 --firmware <phiên_bản>
```

Đóng app Ronin và tắt tự nối BLE trên điện thoại trước khi chạy. Gimbal chỉ nhận một kết nối.

| Công cụ | Bài test |
|---|---|
| `build/bench_g01` | G-T01 đến G-T04 |
| `build/link_loss_test` | G-T05, G-T06 |
| `build/soak_test` | G-T08, G-T09, số liệu API cho G06 |

`api_ms` chỉ là thời gian API, không phải độ trễ motor. G03 và độ trễ motor cần đo bằng camera.

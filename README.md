# Gimbal Driver

Driver độc lập để Cinematic Controller điều khiển DJI RS 3 Mini mà không phụ thuộc BLE. Chỉ dùng thư viện chuẩn Python 3.10+.

**Trạng thái:** mới kiểm thử với Mock và bridge giả lập, chưa chạy trên gimbal thật.

## Tài liệu

| Sản phẩm bàn giao | File | Trạng thái |
|---|---|---|
| Mã nguồn Gimbal Driver | `gimbal_driver/` | Xong, chưa thử trên phần cứng |
| File cấu hình và hướng dẫn cài đặt | `config.example.json`, [docs/INSTALL.md](docs/INSTALL.md) | Xong, chưa thử trên Pi |
| API Documentation | [docs/API.md](docs/API.md) | Xong |
| Demo điều khiển từ Raspberry Pi | `examples/basic_move.py`, `tools/bench_g01.py` | Chờ chạy trên Pi và quay video |
| Bộ test độc lập | `tests/`, `tools/` | Test tự động xong; bài test phần cứng chờ chạy |
| Báo cáo Lock/Follow | [docs/REPORT_LOCK_FOLLOW.md](docs/REPORT_LOCK_FOLLOW.md) | Khung, chờ số liệu |
| Báo cáo độ trễ và độ ổn định BLE | [docs/REPORT_LATENCY_BLE.md](docs/REPORT_LATENCY_BLE.md) | Khung, chờ số liệu |
| Danh sách giới hạn kỹ thuật | [docs/LIMITATIONS.md](docs/LIMITATIONS.md) | Bản đầu, cập nhật sau khi đo |

## Implementation

| Class | Dùng khi |
|---|---|
| `MockGimbalDriver` | Tích hợp sớm, không cần phần cứng |
| `BridgeHttpDriver` | Gọi [rs3mini-bridge](https://github.com/b-rutledge-code/rs3mini-bridge) trên Pi |

## API

```python
from gimbal_driver import MockGimbalDriver

gimbal = MockGimbalDriver()
gimbal.connect()
gimbal.move(pan_dir=1, tilt_dir=0, duration_ms=500)  # hết hạn sẽ tự dừng
gimbal.stop()
gimbal.recenter()
gimbal.get_connection_state()
gimbal.get_gimbal_state()
gimbal.get_capabilities()
gimbal.disconnect()
```

- `move` chỉ nhận hướng -1, 0, 1 và một trục mỗi lần. Tối đa 5000 ms; gọi lại để gia hạn.
- Giá trị góc là tương đối. Giá trị `None` nghĩa là không biết.
- `LinkMonitor` giám sát kết nối và tự nối lại có backoff, giới hạn số lần. Service đã bật sẵn; nếu dùng thư viện trực tiếp thì tự khởi động nó.
- Lệnh `move` cùng hướng gọi dồn dập chỉ gia hạn, cứ 500 ms mới gửi xuống gimbal một lần.
- Sau khi nối lại, driver gửi Stop trước khi nhận lệnh mới nếu lệnh trước bị gián đoạn.
- `disconnect()` của `BridgeHttpDriver` không ngắt BLE, vì bridge giữ kết nối.

## Mã lỗi

| Mã | Ý nghĩa | Xử lý |
|---|---|---|
| `INVALID_ARGUMENT` | Tham số sai | Sửa lời gọi |
| `UNSUPPORTED` | Thiết bị không hỗ trợ | Kiểm tra `get_capabilities()` |
| `NOT_CONNECTED` | Chưa có kết nối | Gọi `connect()` |
| `NOT_READY` | Guard sau kết nối hoặc motor sleep | Thử lại sau |
| `TIMEOUT` | Không phản hồi đúng hạn | Coi như lệnh chưa chắc đã thực thi |
| `AUTH` | Sai token bridge | Kiểm tra `BRIDGE_TOKEN` |
| `BACKEND` | Bridge lỗi hoặc không truy cập được | Kiểm tra dịch vụ |
| `STOP_UNCONFIRMED` | Không xác nhận được lệnh dừng | **Gimbal có thể vẫn quay**, cần xử lý an toàn |

## HTTP service

```bash
cp config.example.json config.json
python -m gimbal_driver --config config.json
```

Không truyền `--config` thì chạy Mock tại `127.0.0.1:8080`. Mở ra mạng LAN bắt buộc phải đặt `token`; gửi token qua header `X-Gimbal-Token` hoặc `?token=`.

| Route | Tác dụng |
|---|---|
| `/gimbal/move?pan=1&tilt=0&duration_ms=500` | Quay; `pan=0&tilt=0` là dừng |
| `/gimbal/stop`, `/gimbal/recenter` | Dừng, căn giữa |
| `/gimbal/status`, `/gimbal/capabilities` | Trạng thái, khả năng |
| `/gimbal/connect`, `/gimbal/disconnect` | Nối, ngắt và thử lại sau khi monitor bỏ cuộc |
| `/gimbal/mode` | Trả 501, chưa có lệnh đã kiểm chứng |

Thiếu `duration_ms` thì lấy 5000 ms. App Windows hiện chưa gửi token, cần sửa trước khi nối vào service có token. Nếu chạy cả bridge với `MOVE_HOLD_MS=2000`, lệnh giữ nên được gọi lại trong vòng 2 giây.

Mẫu systemd nằm ở `deploy/gimbal-driver.service`.

## Kiểm thử

```bash
python -m unittest discover -s tests
```

## Thử trên phần cứng

Trên Pi, chạy `rs3mini-bridge` với `GIMBAL_NAME_RE` là hậu tố tên gimbal, rồi chạy:

```bash
python tools/bench_g01.py --firmware <phiên_bản>
```

Đóng app Ronin và tắt tự nối BLE trên điện thoại trước khi chạy. Gimbal chỉ nhận một kết nối.

| Công cụ | Bài test |
|---|---|
| `tools/bench_g01.py` | G-T01 đến G-T04 |
| `tools/link_loss_test.py` | G-T05, G-T06 |
| `tools/soak_test.py` | G-T08, G-T09, số liệu API cho G06 |

`api_ms` chỉ là thời gian API, không phải độ trễ motor. G03 và độ trễ motor cần đo bằng camera.

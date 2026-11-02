# Gimbal Driver

Driver độc lập để Cinematic Controller điều khiển DJI RS 3 Mini mà không phụ thuộc BLE. Chỉ dùng thư viện chuẩn Python 3.10+.

**Trạng thái:** mới kiểm thử với Mock và bridge giả lập, chưa chạy trên gimbal thật.

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
- `BridgeHttpDriver.poll_link()` cần được gọi định kỳ để phát hiện mất BLE.
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

## Kiểm thử

```bash
python gimbal_driver/tests/test_driver.py
```

## Thử trên phần cứng

Trên Pi, chạy `rs3mini-bridge` với `GIMBAL_NAME_RE` là hậu tố tên gimbal, rồi chạy:

```bash
python gimbal_driver/tools/bench_g01.py --firmware <phiên_bản>
```

Đóng app Ronin và tắt tự nối BLE trên điện thoại trước khi chạy. Gimbal chỉ nhận một kết nối.

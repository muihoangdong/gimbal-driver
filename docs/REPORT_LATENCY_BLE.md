# Báo cáo G06: Độ trễ và độ ổn định BLE

> **Trạng thái:** khung báo cáo, chưa có số liệu. Điền các ô trống sau khi thử.

## 1. Mục tiêu

Cung cấp số liệu để quyết định RS 3 Mini có dùng được cho Auto Tracking hay không.

## 2. Thiết lập

| Mục | Giá trị |
|---|---|
| Ngày thử | |
| Raspberry Pi OS / Python / Node.js | |
| Firmware gimbal / commit bridge | |
| Khoảng cách Pi – gimbal | |
| Cấu hình bridge (`MOVE_STICK_SCALE`, `MOVE_HOLD_MS`) | |

## 3. Phân biệt hai loại thời gian

- **Thời gian API (`api_ms`)**: từ lúc gửi lệnh tới lúc bridge trả lời. Đo bằng `tools/soak_test.py`.
- **Thời gian motor phản ứng**: từ lúc gửi lệnh tới lúc ảnh bắt đầu dịch chuyển. Đo bằng video tốc độ cao hoặc phân tích khung hình webcam trên gimbal, đồng bộ với log lệnh.

Không dùng thời gian API thay cho thời gian motor.

## 4. Kết quả

| Phép đo | Số lần | Trung vị | p95 | Max | Công cụ |
|---|---|---|---|---|---|
| Thời gian kết nối (s) | | | | | `bench_g01.py` |
| `api_ms` lệnh move | | | | | `soak_test.py` |
| Trễ lệnh → motor chuyển động (ms) | | | | | Video |
| Trễ Stop → motor dừng (ms) | | | | | Video |
| Đáp ứng đảo chiều (ms) | | | | | Video |
| Thời gian phát hiện mất BLE (s) | | | | | `link_loss_test.py` |
| Thời gian nối lại (s) | | | | | `link_loss_test.py` |

**Dao động khi gửi lệnh liên tục:**

## 5. Ổn định dài hạn

| Test ID | Điều kiện | Thời lượng | Lệnh OK | Lỗi | Mất kết nối | Kết quả |
|---|---|---|---|---|---|---|
| G-T08 | Không Wi-Fi tải | 30 phút | | | | Pass / Fail |
| G-T09 | Pi stream video + Wi-Fi | 30 phút | | | | Pass / Fail |

## 6. Mất kết nối

| Test ID | Điều kiện | Gimbal tiếp tục quay? | Kết quả | Ghi chú |
|---|---|---|---|---|
| G-T05 | Mất BLE khi đang quay | Có / Không | Pass / Fail | |
| G-T06 | Nối lại sau mất BLE | — | Pass / Fail | |

## 7. Kết luận và cấu hình đề xuất

- Có dùng được cho Auto Tracking không:
- Chu kỳ gửi lệnh và `duration_ms` đề xuất:
- Cấu hình bridge đề xuất:
- Cập nhật L30–L34 trong LIMITATIONS.md

## 8. Minh chứng

- Log: `soak.csv`, `bench_g01.csv`, journal của bridge
- Biểu đồ:
- Video:

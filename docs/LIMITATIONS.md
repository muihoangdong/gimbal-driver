# Danh sách giới hạn kỹ thuật

Cột **Trạng thái**: `Xác nhận` = đã đo trên RS 3 Mini của nhóm; `Theo tài liệu` = lấy từ rs3mini-bridge hoặc DJI, chưa đo; `Chưa xác minh` = giả định cần kiểm tra. Cập nhật bảng này sau mỗi đợt thử.

## Điều khiển

| # | Giới hạn | Trạng thái | Nguồn / bài test |
|---|---|---|---|
| L01 | Chỉ điều khiển theo hướng (joystick), không có tốc độ liên tục | Theo tài liệu | rs3mini-bridge API |
| L02 | Không có góc tuyệt đối; góc đọc về chỉ là tương đối | Theo tài liệu | rs3mini-bridge API |
| L03 | Mỗi lệnh chỉ một trục (pan hoặc tilt) | Theo tài liệu | `/move/hold` nhận một trục |
| L04 | Biên độ joystick cố định theo `MOVE_STICK_SCALE` (mặc định 0,05) | Theo tài liệu | Chưa đo tốc độ quay thực tế (G06) |
| L05 | Chưa đổi được chế độ Lock/Follow từ Pi | Chưa xác minh | G03, G-T07 |
| L06 | Lệnh quay bị chặn ~5 s sau khi bridge khởi động và ~8 s sau khi nối BLE | Theo tài liệu | `STARTUP_GUARD_MS`, `CONNECT_GUARD_MS` |
| L07 | Driver tự giới hạn mỗi lệnh quay tối đa 5000 ms | Xác nhận (Mock) | tests |

## Kết nối BLE

| # | Giới hạn | Trạng thái | Nguồn / bài test |
|---|---|---|---|
| L10 | Gimbal chỉ nhận một central; phải tắt app Ronin và app Android | Theo tài liệu | G-T01 |
| L11 | Kết nối có thể mất 30–45 s | Theo tài liệu | G-T01, G06 |
| L12 | Gimbal có thể giữ kết nối cũ sau khi rớt; phải tắt bật nguồn | Theo tài liệu | G-T06 |
| L13 | Tầm BLE ngắn, chưa đo trên Pi 4 | Chưa xác minh | G06 |
| L14 | Khi mất BLE, không đảm bảo lệnh Stop tới gimbal | Theo yêu cầu PDF | G-T05 |
| L15 | Hành vi gimbal khi mất BLE giữa lúc quay chưa biết | Chưa xác minh | G-T05 |
| L16 | Ảnh hưởng của Wi-Fi/video tới BLE chưa đo | Chưa xác minh | G-T09 |

## Giao thức và phụ thuộc

| # | Giới hạn | Trạng thái | Ghi chú |
|---|---|---|---|
| L20 | Giao thức BLE do cộng đồng dịch ngược, không có SDK chính thức cho RS 3 Mini | Theo tài liệu | Firmware mới có thể làm hỏng |
| L21 | Phụ thuộc rs3mini-bridge (1 commit, mới thử trên Pi Zero 2 W) | Theo tài liệu | Ghi mã commit trong INSTALL.md |
| L22 | Battery và telemetry có thể cũ (`stale`) sau 3 s không cập nhật | Theo tài liệu | `TELEMETRY_STALE_MS` |

## Hiệu năng (điền sau G06)

| # | Đại lượng | Giá trị đo | Điều kiện |
|---|---|---|---|
| L30 | Thời gian kết nối | | |
| L31 | Trễ từ lệnh đến chuyển động thật | | |
| L32 | Trễ dừng | | |
| L33 | Tốc độ quay ứng với lệnh joystick | | |
| L34 | Tỷ lệ mất kết nối / giờ | | |
| L35 | Tốc độ xoay thân xe tối đa mà gimbal vẫn giữ hướng | | G03 |

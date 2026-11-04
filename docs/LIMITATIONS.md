# Danh sách giới hạn kỹ thuật

Cột **Trạng thái**: `Xác nhận` = đã đo trên RS 3 Mini của nhóm; `Theo tài liệu` = lấy từ rs3mini-bridge hoặc DJI, chưa đo; `Chưa xác minh` = giả định cần kiểm tra. Cập nhật bảng này sau mỗi đợt thử.

## Điều khiển

| # | Giới hạn | Trạng thái | Nguồn / bài test |
|---|---|---|---|
| L01 | Chỉ điều khiển theo hướng (joystick), không có tốc độ liên tục | Theo tài liệu | rs3mini-bridge API |
| L02 | Không có góc tuyệt đối; góc đọc về chỉ là tương đối | Theo tài liệu | rs3mini-bridge API |
| L03 | Mỗi lệnh chỉ một trục (pan hoặc tilt) | Theo tài liệu | `/move/hold` nhận một trục |
| L04 | Biên độ joystick cố định theo `MOVE_STICK_SCALE` của bridge. Mặc định 0,05 làm pan chỉ ~1°/s, lệnh 0,5 s gần như không thấy; tilt vẫn thấy được. Dùng 0,3 thì pan ~5°/s | Xác nhận (2026-11-04) | Bench G01 lần 1 và 2; tốc độ ước từ telemetry tương đối |
| L05 | Chưa đổi được chế độ Lock/Follow từ Pi | Chưa xác minh | G03, G-T07 |
| L06 | Lệnh quay bị chặn ~5 s sau khi bridge khởi động và ~8 s sau khi nối BLE | Theo tài liệu | `STARTUP_GUARD_MS`, `CONNECT_GUARD_MS` |
| L07 | Driver tự giới hạn mỗi lệnh quay tối đa 5000 ms | Xác nhận (Mock) | tests |
| L08 | Không hiệu chỉnh động cơ (Auto Tune / System Calibration) từ Pi; nút Calib chỉ đặt mốc 0 phần mềm, mất khi service khởi động lại | Xác nhận (2026-11-04) | Bridge không có lệnh; mốc thử trên gimbal thật |

## Kết nối BLE

| # | Giới hạn | Trạng thái | Nguồn / bài test |
|---|---|---|---|
| L10 | Gimbal chỉ nhận một central; phải tắt app Ronin và app Android | Theo tài liệu | G-T01 |
| L11 | Kết nối có thể mất 30–45 s. Trên Pi 4 của nhóm: bridge nối BLE trong 421 ms, sẵn sàng sau khoảng 5 s, cho phép lệnh quay sau ~10 s (guard 8 s) | Xác nhận một lần (2026-11-04) | Log bridge; cần đo lặp lại ở G06 |
| L17 | Chip Bluetooth UART của Pi 4 từng treo (`Frame reassembly failed`, `tx timeout`) vài giây sau khi `gamesir_robot.py` khởi động; khởi động lại `hciuart` không đủ, phải reboot | Xác nhận một lần (2026-11-04) | Chưa rõ nguyên nhân; cần thử chạy gimbal cùng tay cầm |
| L12 | Gimbal có thể giữ kết nối cũ sau khi rớt; phải tắt bật nguồn. Trên Pi 4: sau khi khởi động lại bridge, lần nối đầu báo "Gimbal not found in 30s", lần sau mới nối được | Xác nhận một phần (2026-11-04) | G-T06 |
| L18 | Sau khi chỉnh trục bằng nút/cần trên RS 3 Mini, gimbal bỏ qua mọi lệnh BLE (quay, về giữa) dù BLE, telemetry và motor vẫn bình thường; `/telemetry/refresh` không khôi phục, khởi động lại bridge (phiên BLE mới) thì khôi phục | Xác nhận một lần (2026-11-04) | Chưa rõ nguyên nhân; không chỉnh tay khi đang điều khiển từ Pi |
| L13 | Tầm BLE ngắn, chưa đo trên Pi 4 | Chưa xác minh | G06 |
| L14 | Khi mất BLE, không đảm bảo lệnh Stop tới gimbal | Theo yêu cầu PDF | G-T05 |
| L15 | Hành vi gimbal khi mất BLE giữa lúc quay chưa biết | Chưa xác minh | G-T05 |
| L16 | Ảnh hưởng của Wi-Fi/video tới BLE chưa đo | Chưa xác minh | G-T09 |

## Giao thức và phụ thuộc

| # | Giới hạn | Trạng thái | Ghi chú |
|---|---|---|---|
| L20 | Giao thức BLE do cộng đồng dịch ngược, không có SDK chính thức cho RS 3 Mini | Theo tài liệu | Firmware mới có thể làm hỏng |
| L21 | Phụ thuộc rs3mini-bridge (1 commit; tác giả thử trên Pi Zero 2 W). Nhóm đã chạy được commit `f3c3c3d` trên Pi 4, Ubuntu 22.04, Node 22 | Xác nhận (2026-11-04) | Ghi mã commit trong INSTALL.md |
| L22 | Battery và telemetry có thể cũ (`stale`) sau 3 s không cập nhật | Theo tài liệu | `TELEMETRY_STALE_MS` |

## Hiệu năng (điền sau G06)

| # | Đại lượng | Giá trị đo | Điều kiện |
|---|---|---|---|
| L30 | Thời gian kết nối | BLE 421 ms; bridge `OK` sau ~5 s, nhận lệnh quay sau ~10 s | Một lần, gimbal cách Pi gần, RSSI −62 dBm |
| L31 | Trễ từ lệnh đến chuyển động thật | | |
| L32 | Trễ dừng | | |
| L33 | Tốc độ quay ứng với lệnh joystick | Pan ~1°/s ở scale 0,05; ~5°/s ở scale 0,3 | Ước từ telemetry tương đối, một lần đo |
| L34 | Tỷ lệ mất kết nối / giờ | | |
| L35 | Tốc độ xoay thân xe tối đa mà gimbal vẫn giữ hướng | | G03 |

# Báo cáo G03: So sánh chế độ Lock / Follow khi xe xoay

> **Trạng thái:** khung báo cáo, chưa có số liệu. Điền các ô trống sau khi thử.

## 1. Mục tiêu

Đánh giá khả năng gimbal giữ hướng camera khi thân xe xoay, để chọn chế độ dùng cho Auto Tracking.

## 2. Thiết lập

| Mục | Giá trị |
|---|---|
| Ngày thử | |
| Firmware gimbal | |
| Camera và tải trọng | |
| Cài đặt gimbal (Pan Speed, Deadband, Smoothness) | |
| Cách đổi chế độ | Trên gimbal / qua app Ronin / qua Pi |
| Marker | Ảnh in cố định, cách camera ___ m |

## 3. Phương pháp

1. Đặt xe đứng yên, camera nhìn thẳng marker, ghi vị trí marker trong ảnh (pixel).
2. Cho xe xoay tại chỗ ở tốc độ đặt trước, thấp trước rồi tăng dần, trong khu vực có giám sát.
3. Quay video từ camera trên gimbal; đo độ lệch tâm marker theo thời gian.
4. Đổi pixel sang độ: `góc ≈ lệch_px × FOV_ngang / chiều_rộng_ảnh`.
5. Lặp lại mỗi chế độ ít nhất 3 lần.

## 4. Kết quả

| Chế độ | Tốc độ xoay xe (°/s) | Lệch tối đa (°) | Lệch trung bình (°) | Thời gian ổn định (s) | Giật/dao động | Ghi chú |
|---|---|---|---|---|---|---|
| Lock | | | | | | |
| Pan Follow | | | | | | |
| Pan-Tilt Follow | | | | | | |
| FPV | | | | | | |

**Tốc độ xoay xe tối đa mà gimbal còn giữ được hướng:** ___ °/s

**Ảnh hưởng của cài đặt gimbal:**

## 5. Bài test G-T07

| Test ID | Điều kiện | Kết quả | Ghi chú lỗi |
|---|---|---|---|
| G-T07 | | Pass / Fail | |

## 6. Khuyến nghị cho Auto Tracking

- Chế độ đề xuất:
- Lý do:
- Giới hạn cần báo Project Lead (cập nhật L05, L35 trong LIMITATIONS.md):

## 7. Minh chứng

- Video:
- Dữ liệu đo:

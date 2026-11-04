# Hướng dẫn cài đặt

Cài trên Raspberry Pi 4 để điều khiển DJI RS 3 Mini. Muốn thử API khi chưa có gimbal thì chỉ cần Python 3.10+ (mục 7).

> **Trạng thái:** các bước dưới chưa được chạy trọn vẹn trên Pi 4 và RS 3 Mini của nhóm. Ghi lại mọi chỗ khác thực tế vào mục *Nhật ký cài đặt* cuối file.

## 1. Yêu cầu

| Thành phần | Yêu cầu |
|---|---|
| Phần cứng | Raspberry Pi 4, DJI RS 3 Mini đã cân bằng |
| Hệ điều hành | Raspberry Pi OS (64-bit khuyến nghị) |
| Phần mềm | Python 3.10+, Node.js 20+, BlueZ, git |
| Mạng | Pi và máy điều khiển cùng mạng LAN |

## 2. Cài gói hệ thống

```bash
sudo apt update
sudo apt install -y git python3 bluetooth bluez libbluetooth-dev libudev-dev build-essential
sudo systemctl enable --now bluetooth
sudo rfkill unblock bluetooth
python3 --version
node -v
```

Nếu `node -v` thấp hơn 20 hoặc chưa có, cài Node.js 20+ trước khi tiếp tục.

## 3. Cài rs3mini-bridge

Bridge giữ kết nối BLE với gimbal. Đây là dự án riêng, MIT license, không do nhóm viết.

```bash
git clone https://github.com/b-rutledge-code/rs3mini-bridge.git ~/rs3mini-bridge
cd ~/rs3mini-bridge
npm ci
git rev-parse HEAD   # ghi mã commit vào Nhật ký cài đặt
```

Nếu bridge báo thiếu quyền Bluetooth:

```bash
sudo setcap cap_net_raw+eip "$(readlink -f "$(which node)")"
```

## 4. Tìm tên gimbal

Đóng app Ronin và tắt Bluetooth của điện thoại đang nối gimbal; gimbal chỉ nhận một kết nối.

```bash
bluetoothctl --timeout 15 scan on
```

Tìm dòng `DJI RS3 MINI-XXXX`; `XXXX` là hậu tố dùng ở bước sau. Không cần ghép đôi.

## 5. Chạy và kiểm tra bridge

```bash
cd ~/rs3mini-bridge
GIMBAL_NAME_RE=XXXX MOVE_HOLD_MS=2000 npm start
```

Ở terminal khác (kết nối có thể mất 30–45 giây):

```bash
curl http://127.0.0.1:8787/health/label   # "OK" là đã nối
```

`MOVE_HOLD_MS=2000` để bridge tự về trung tính nếu không nhận lệnh giữ trong 2 giây.

## 6. Cài Gimbal Driver

```bash
git clone https://github.com/muihoangdong/gimbal-driver.git ~/gimbal-driver
cd ~/gimbal-driver
python3 -m unittest discover -s tests
cp config.example.json config.json
nano config.json   # đổi token, chọn driver "bridge"
python3 -m gimbal_driver --config config.json
```

Kiểm tra từ máy khác trong LAN:

```bash
curl -H "X-Gimbal-Token: <token>" http://<IP_PI>:8080/gimbal/status
```

## 7. Chạy thử không cần gimbal

Trên Windows, Linux hoặc macOS có Python 3.10+:

```bash
python -m gimbal_driver            # Mock tại 127.0.0.1:8080
python examples/basic_move.py
```

## 8. Tự khởi động bằng systemd

Sửa `User` và đường dẫn trong `deploy/gimbal-driver.service` nếu Pi không dùng user `pi`.

```bash
sudo cp deploy/gimbal-driver.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now gimbal-driver
journalctl -u gimbal-driver -f
```

Bridge cũng cần chạy như service; dùng `scripts/install.sh` hoặc `rs3mini-bridge.service` trong repo bridge.

## 9. Cấu hình

| Khóa | Mặc định | Ý nghĩa |
|---|---|---|
| `driver` | `mock` | `mock` hoặc `bridge` |
| `bridge_url` | `http://127.0.0.1:8787` | Địa chỉ rs3mini-bridge |
| `bridge_token` | `null` | `BRIDGE_TOKEN` nếu bridge có đặt |
| `host` | `127.0.0.1` | `0.0.0.0` để mở ra LAN |
| `port` | `8080` | Cổng HTTP service |
| `token` | `null` | Bắt buộc khi `host` không phải localhost |
| `log_file` | `null` | File log, xoay vòng 5 MB × 3 |
| `connect_timeout_s` | `60` | Thời gian chờ mỗi lần kết nối |
| `monitor_interval_s` | `1.0` | Chu kỳ kiểm tra liên kết |
| `reconnect_attempts` | `5` | Số lần nối lại trước khi bỏ cuộc |

## 10. Xử lý sự cố

| Hiện tượng | Cách xử lý |
|---|---|
| `"NO BLE"` | Bật Bluetooth, kiểm tra `GIMBAL_NAME_RE` |
| Kẹt `"CONN…"` / `"NO GIMB"` | Đóng app Ronin, đưa Pi lại gần, tắt bật gimbal |
| Nối được một lần rồi không nối lại | Gimbal giữ kết nối cũ: tắt bật nguồn gimbal |
| Lệnh quay trả 503 `NOT_READY` | Chờ khoảng 8 giây sau khi nối; kiểm tra motor không ở chế độ sleep |
| Service không khởi động | Xem `journalctl -u gimbal-driver`; kiểm tra `token` khi mở ra LAN |

## Nhật ký cài đặt

| Ngày | Phiên bản Pi OS | Firmware gimbal | Commit bridge | Ghi chú |
|---|---|---|---|---|
| | | | | |

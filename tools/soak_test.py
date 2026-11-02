"""G-T08/G-T09: chay lien tuc, ghi thoi gian API va loi ra CSV.

api_ms la thoi gian HTTP tra loi, KHONG phai do tre motor. Khi test G-T09, bat video/Wi-Fi
song song va dat --label de so sanh giua cac lan chay.
"""
import argparse
import csv
import statistics
import sys
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import BridgeHttpDriver, ConnectionState, GimbalError, MockGimbalDriver


def timed(writer, label, command, fn, samples, errors):
    started = time.monotonic()
    error = ""
    try:
        fn()
    except GimbalError as e:
        error = e.code.value
        errors.append(error)
    api_ms = round((time.monotonic() - started) * 1000, 1)
    if not error:
        samples.append(api_ms)
    writer.writerow([datetime.now().isoformat(timespec="milliseconds"), label, command,
                     api_ms, error])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8787")
    parser.add_argument("--token")
    parser.add_argument("--mock", action="store_true")
    parser.add_argument("--minutes", type=float, default=30)
    parser.add_argument("--pulse-ms", type=int, default=300)
    parser.add_argument("--pause-s", type=float, default=2.0)
    parser.add_argument("--label", default="baseline")
    parser.add_argument("--out", default="soak.csv")
    args = parser.parse_args()

    driver = MockGimbalDriver() if args.mock else BridgeHttpDriver(args.url, token=args.token)
    driver.connect()
    samples, errors = [], []
    link_losses = 0
    end = time.monotonic() + args.minutes * 60
    direction = 1
    with open(args.out, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["time", "label", "command", "api_ms", "error"])
        while time.monotonic() < end:
            # Dao chieu moi chu ky de gimbal khong troi dan ve mot phia.
            timed(writer, args.label, f"pan{direction:+d}",
                  lambda: driver.move(direction, 0, args.pulse_ms), samples, errors)
            time.sleep(args.pulse_ms / 1000 + 0.2)
            direction = -direction
            f.flush()
            if driver.check_link() is not ConnectionState.CONNECTED:
                link_losses += 1
                writer.writerow([datetime.now().isoformat(timespec="milliseconds"),
                                 args.label, "LINK_LOST", "", str(driver.get_last_error())])
                try:
                    driver.connect()
                except GimbalError as e:
                    errors.append(e.code.value)
                    time.sleep(5)
            time.sleep(args.pause_s)
    driver.disconnect()

    print(f"Lenh OK: {len(samples)}, loi: {len(errors)}, mat link: {link_losses}")
    if samples:
        ordered = sorted(samples)
        print(f"api_ms trung vi {statistics.median(ordered)}, "
              f"p95 {ordered[int(len(ordered) * 0.95) - 1]}, max {ordered[-1]}")


if __name__ == "__main__":
    main()

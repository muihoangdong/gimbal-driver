"""Kiem thu ban tren RS 3 Mini: nguoi quan sat cham Pass/Fail, ket qua ghi ra CSV.

Chay tren Pi sau khi rs3mini-bridge da chay. Gimbal phai duoc can bang va hanh trinh quay
khong vuong vat gi; luon dung sat de tat nguon neu gimbal khong dung.
"""
import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import BridgeHttpDriver, GimbalError

PULSE_MS = 500


def run_step(writer, test_id, name, action, note_hint):
    input(f"\n[{test_id}] {name}\n  Quan sat: {note_hint}\n  Enter de chay...")
    started = time.monotonic()
    error = ""
    try:
        action()
    except GimbalError as e:
        error = str(e)
        print(f"  LOI: {error}")
    api_ms = round((time.monotonic() - started) * 1000)
    passed = input("  Dat yeu cau? (y/n): ").strip().lower() == "y"
    note = input("  Ghi chu: ").strip()
    writer.writerow([datetime.now().isoformat(timespec="seconds"), test_id, name,
                     "Pass" if passed else "Fail", api_ms, error, note])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8787")
    parser.add_argument("--token")
    parser.add_argument("--firmware", required=True, help="Phien ban firmware gimbal")
    parser.add_argument("--out", default="bench_g01.csv")
    args = parser.parse_args()

    driver = BridgeHttpDriver(args.url, token=args.token)
    with open(args.out, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["time", "test_id", "name", "result", "api_ms", "error", "note"])
        writer.writerow([datetime.now().isoformat(timespec="seconds"), "ENV",
                         f"firmware={args.firmware}", "", "", "", args.url])

        def pulse(pan, tilt):
            def act():
                driver.move(pan, tilt, PULSE_MS)
                time.sleep(PULSE_MS / 1000 + 0.3)
            return act

        def show_state():
            print(" ", driver.get_gimbal_state())

        run_step(writer, "G-T01", "Ket noi qua bridge", lambda: driver.connect(90),
                 "Bridge bao OK; api_ms la thoi gian cho ket noi")
        run_step(writer, "G-T02a", "Pan trai", pulse(-1, 0), "Quay trai roi tu dung")
        run_step(writer, "G-T02b", "Pan phai", pulse(1, 0), "Quay phai roi tu dung")
        run_step(writer, "G-T03a", "Tilt len", pulse(0, 1), "Ngang len roi tu dung")
        run_step(writer, "G-T03b", "Tilt xuong", pulse(0, -1), "Cui xuong roi tu dung")

        def move_then_stop():
            driver.move(1, 0, 3000)
            time.sleep(0.5)
            driver.stop()

        run_step(writer, "G-T04a", "Stop giua chung", move_then_stop,
                 "Dung sau khoang 0,5 giay, khong doi het 3 giay")
        run_step(writer, "G-T04b", "Recenter", driver.recenter, "Ve giua")
        run_step(writer, "STATE", "Doc telemetry", show_state,
                 "So voi thuc te; goc chi la tuong doi")

    print(f"\nDa ghi ket qua vao {args.out}. Bai G-T05/G-T06 dung tools/link_loss_test.py.")


if __name__ == "__main__":
    main()

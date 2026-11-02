"""G-T05/G-T06: mat BLE khi gimbal dang quay, ghi thoi gian phat hien va noi lai.

Nguoi van hanh tu gay mat BLE (che chan, dung bluetooth tren Pi) va dung sat de tat nguon gimbal.
"""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import BridgeHttpDriver, ConnectionState, GimbalError


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8787")
    parser.add_argument("--token")
    parser.add_argument("--wait-s", type=float, default=120)
    args = parser.parse_args()

    driver = BridgeHttpDriver(args.url, token=args.token)
    driver.connect()
    input("Bam Enter de quay; ngay sau do hay gay mat BLE...")
    driver.move(1, 0, 3000)
    started = time.monotonic()

    while driver.check_link() is ConnectionState.CONNECTED:
        if time.monotonic() - started > args.wait_s:
            print("Khong phat hien mat link; hanh dong ngat BLE chua co tac dung.")
            driver.disconnect()
            return
        time.sleep(0.1)
    print(f"G-T05: phat hien sau {time.monotonic() - started:.1f}s, "
          f"loi: {driver.get_last_error()}")
    print("Quan sat: gimbal CO tiep tuc quay sau khi mat BLE khong? Ghi vao bao cao.")

    input("Khoi phuc BLE roi bam Enter...")
    recovered = time.monotonic()
    try:
        driver.connect(args.wait_s)
    except GimbalError as e:
        print(f"G-T06: Fail, {e}")
        return
    print(f"G-T06: noi lai sau {time.monotonic() - recovered:.1f}s va da gui Stop")
    driver.disconnect()


if __name__ == "__main__":
    main()

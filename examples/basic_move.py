"""Vi du dung API; them --bridge de chay voi rs3mini-bridge that."""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import BridgeHttpDriver, GimbalError, MockGimbalDriver


def main():
    gimbal = BridgeHttpDriver() if "--bridge" in sys.argv else MockGimbalDriver()
    try:
        gimbal.connect()
        print(gimbal.get_capabilities())
        gimbal.move(pan_dir=1, tilt_dir=0, duration_ms=500)
        time.sleep(0.8)
        print(gimbal.get_gimbal_state())
        gimbal.recenter()
    except GimbalError as e:
        print(f"Loi {e.code.value}: {e}")
    finally:
        gimbal.disconnect()


if __name__ == "__main__":
    main()

"""Gimbal gia lap de cac nhom tich hop khi chua co phan cung."""
import time
from typing import Optional

from .base import Capabilities, ErrorCode, GimbalDriver, GimbalError, GimbalState


class MockGimbalDriver(GimbalDriver):
    def __init__(self, rate_deg_s: float = 30.0):
        super().__init__()
        self.rate_deg_s = rate_deg_s
        self.pan_deg = 0.0
        self.tilt_deg = 0.0
        self.link_up = True
        # Mo phong dong co: khi mat link, lenh Stop khong toi duoc gimbal.
        self.physically_moving = False
        self._dir = (0, 0)
        self._last_update = time.monotonic()
        self.commands = []

    def get_capabilities(self) -> Capabilities:
        return Capabilities(telemetry=True, angle_reference="relative")

    def get_gimbal_state(self) -> GimbalState:
        self._integrate()
        return GimbalState(moving=self.physically_moving, pan_deg=self.pan_deg,
                           tilt_deg=self.tilt_deg, roll_deg=0.0, battery_percent=100,
                           motor="AWAKE", telemetry_stale=not self.link_up)

    def drop_link(self) -> None:
        """Gia lap mat Bluetooth; gimbal giu nguyen chuyen dong dang co."""
        self.link_up = False

    def restore_link(self) -> None:
        self.link_up = True

    def _integrate(self) -> None:
        now = time.monotonic()
        dt, self._last_update = now - self._last_update, now
        if self.physically_moving:
            self.pan_deg += self._dir[0] * self.rate_deg_s * dt
            self.tilt_deg += self._dir[1] * self.rate_deg_s * dt

    def _check_link(self) -> None:
        if not self.link_up:
            raise GimbalError(ErrorCode.NOT_CONNECTED, "mock: mat link")

    def _open(self, timeout_s: Optional[float]) -> None:
        self._check_link()

    def _close(self) -> None:
        pass

    def _send_move(self, pan_dir: int, tilt_dir: int) -> None:
        self._check_link()
        self._integrate()
        self._dir = (pan_dir, tilt_dir)
        self.physically_moving = True
        self.commands.append(("move", pan_dir, tilt_dir))

    def _send_stop(self) -> None:
        self._check_link()
        self._integrate()
        self._dir = (0, 0)
        self.physically_moving = False
        self.commands.append(("stop",))

    def _send_recenter(self) -> None:
        self._check_link()
        self._dir = (0, 0)
        self.physically_moving = False
        self.pan_deg = self.tilt_deg = 0.0
        self.commands.append(("recenter",))

"""Giam sat lien ket va reconnect co gioi han/backoff; khong phu thuoc implementation."""
import logging
import threading
from typing import Callable, Optional

from .base import ConnectionState, GimbalDriver, GimbalError

log = logging.getLogger("gimbal_driver.monitor")


class LinkMonitor:
    """on_event(ten, chi_tiet) bao len Cinematic Controller: LINK_LOST, RECONNECTED, GAVE_UP."""

    def __init__(self, driver: GimbalDriver, interval_s: float = 1.0, max_attempts: int = 5,
                 base_backoff_s: float = 2.0, max_backoff_s: float = 30.0,
                 connect_timeout_s: float = 60.0,
                 on_event: Optional[Callable[[str, str], None]] = None):
        self.driver = driver
        self.interval_s = interval_s
        self.max_attempts = max_attempts
        self.base_backoff_s = base_backoff_s
        self.max_backoff_s = max_backoff_s
        self.connect_timeout_s = connect_timeout_s
        self.on_event = on_event
        self.gave_up = False
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="gimbal-monitor", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=5)
            self._thread = None

    def reset(self) -> None:
        """Cho phep thu lai sau GAVE_UP, vi du khi nguoi van hanh da bat lai gimbal."""
        self.gave_up = False

    def _emit(self, name: str, detail: str = "") -> None:
        log.warning("%s %s", name, detail)
        if self.on_event is not None:
            try:
                self.on_event(name, detail)
            except Exception:
                log.exception("on_event loi")

    def _run(self) -> None:
        while not self._stop.wait(self.interval_s):
            if self.gave_up or self.driver.check_link() is not ConnectionState.FAULT:
                continue
            error = self.driver.get_last_error()
            self._emit("LINK_LOST", str(error) if error else "")
            self._reconnect()

    def _reconnect(self) -> None:
        for attempt in range(self.max_attempts):
            delay = min(self.base_backoff_s * (2 ** attempt), self.max_backoff_s)
            if self._stop.wait(delay):
                return
            try:
                self.driver.connect(self.connect_timeout_s)
            except GimbalError as e:
                log.warning("reconnect %d/%d that bai: %s", attempt + 1, self.max_attempts, e)
                continue
            self._emit("RECONNECTED", f"lan {attempt + 1}")
            return
        self.gave_up = True
        self._emit("GAVE_UP", f"sau {self.max_attempts} lan")

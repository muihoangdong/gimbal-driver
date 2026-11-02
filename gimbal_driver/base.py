"""Hop dong chung cua Gimbal Driver: khong chua logic tracking hay giao thuc BLE."""
import logging
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum
from typing import Optional

log = logging.getLogger("gimbal_driver")

# Moi lenh chuyen dong phai co thoi han; driver tu dung khi het han.
MAX_MOVE_DURATION_MS = 5000
# Lenh cung huong goi lien tuc chi gia han noi bo, khong gui xuong gimbal moi lan.
MOVE_RESEND_MS = 500


class ConnectionState(Enum):
    DISCONNECTED = "DISCONNECTED"
    CONNECTING = "CONNECTING"
    CONNECTED = "CONNECTED"
    FAULT = "FAULT"


class ErrorCode(Enum):
    INVALID_ARGUMENT = "INVALID_ARGUMENT"
    UNSUPPORTED = "UNSUPPORTED"
    NOT_CONNECTED = "NOT_CONNECTED"
    NOT_READY = "NOT_READY"  # guard sau ket noi, motor sleep...
    TIMEOUT = "TIMEOUT"
    AUTH = "AUTH"
    BACKEND = "BACKEND"
    STOP_UNCONFIRMED = "STOP_UNCONFIRMED"  # gimbal co the van dang quay


class GimbalError(Exception):
    def __init__(self, code: ErrorCode, message: str = ""):
        super().__init__(f"{code.value}: {message}" if message else code.value)
        self.code = code


@dataclass(frozen=True)
class Capabilities:
    """Chi khai bao dieu da duoc ho tro; mac dinh la khong ho tro."""
    direction_control: bool = True
    speed_control: bool = False
    absolute_angle: bool = False
    simultaneous_axes: bool = False
    recenter: bool = True
    telemetry: bool = False
    angle_reference: Optional[str] = None  # "relative": khong phai huong da hieu chuan
    verified_on_hardware: bool = False


@dataclass(frozen=True)
class GimbalState:
    """Telemetry co the thieu: None nghia la khong biet, khong phai bang 0."""
    moving: bool
    pan_deg: Optional[float] = None
    tilt_deg: Optional[float] = None
    roll_deg: Optional[float] = None
    battery_percent: Optional[int] = None
    motor: str = "UNKNOWN"
    telemetry_stale: bool = True


class GimbalDriver(ABC):
    """Lop API ma Cinematic Controller goi; lop con chi cai dat _send_*."""

    def __init__(self):
        self._lock = threading.RLock()
        self._state = ConnectionState.DISCONNECTED
        self._last_error: Optional[GimbalError] = None
        self._moving = False
        self._last_move = None
        self._last_send = 0.0
        self._move_generation = 0
        self._deadline_timer: Optional[threading.Timer] = None

    # --- Phan moi implementation phai cung cap ---
    @abstractmethod
    def get_capabilities(self) -> Capabilities: ...

    @abstractmethod
    def get_gimbal_state(self) -> GimbalState: ...

    @abstractmethod
    def _open(self, timeout_s: float) -> None: ...

    @abstractmethod
    def _close(self) -> None: ...

    @abstractmethod
    def _send_move(self, pan_dir: int, tilt_dir: int) -> None: ...

    @abstractmethod
    def _send_stop(self) -> None: ...

    @abstractmethod
    def _send_recenter(self) -> None: ...

    # --- API cong khai ---
    def connect(self, timeout_s: float = 60.0) -> None:
        with self._lock:
            self._state = ConnectionState.CONNECTING
            try:
                self._open(timeout_s)
            except GimbalError as e:
                self._fail(e)
                raise
            self._state = ConnectionState.CONNECTED
            if self._moving:
                # Noi lai sau khi mat link giua luc quay: dung truoc khi nhan lenh moi.
                try:
                    self._send_stop()
                except GimbalError as e:
                    self._fail(GimbalError(ErrorCode.STOP_UNCONFIRMED, str(e)))
                    raise self._last_error from e
                self._moving = False
            self._last_error = None
            log.info("connected")

    def disconnect(self) -> None:
        with self._lock:
            try:
                if self._moving:
                    self.stop()
            except GimbalError as e:
                log.warning("stop truoc khi ngat that bai: %s", e)
            self._close()
            self._state = ConnectionState.DISCONNECTED
            log.info("disconnected")

    def check_link(self) -> ConnectionState:
        """Kiem tra lien ket that; duoc LinkMonitor goi dinh ky."""
        return self.get_connection_state()

    def get_connection_state(self) -> ConnectionState:
        with self._lock:
            return self._state

    def get_last_error(self) -> Optional[GimbalError]:
        with self._lock:
            return self._last_error

    def move(self, pan_dir: int, tilt_dir: int, duration_ms: int) -> None:
        """Quay theo huong {-1, 0, 1} trong duration_ms; goi lai de gia han."""
        if pan_dir not in (-1, 0, 1) or tilt_dir not in (-1, 0, 1):
            raise GimbalError(ErrorCode.INVALID_ARGUMENT, "huong phai la -1, 0 hoac 1")
        if pan_dir == 0 and tilt_dir == 0:
            raise GimbalError(ErrorCode.INVALID_ARGUMENT, "dung stop() de dung")
        if not 0 < duration_ms <= MAX_MOVE_DURATION_MS:
            raise GimbalError(ErrorCode.INVALID_ARGUMENT,
                              f"duration_ms phai trong (0, {MAX_MOVE_DURATION_MS}]")
        if pan_dir and tilt_dir and not self.get_capabilities().simultaneous_axes:
            raise GimbalError(ErrorCode.UNSUPPORTED, "khong ho tro quay 2 truc cung luc")
        with self._lock:
            self._require_connected()
            now = time.monotonic()
            duplicate = (self._moving and self._last_move == (pan_dir, tilt_dir)
                         and (now - self._last_send) * 1000 < MOVE_RESEND_MS)
            if not duplicate:
                try:
                    self._send_move(pan_dir, tilt_dir)
                except GimbalError as e:
                    self._fail(e)
                    raise
                self._last_move = (pan_dir, tilt_dir)
                self._last_send = now
            self._moving = True
            self._arm_deadline(duration_ms)

    def stop(self) -> None:
        with self._lock:
            self._cancel_deadline()
            try:
                self._require_connected()
                self._send_stop()
            except GimbalError as e:
                unconfirmed = GimbalError(ErrorCode.STOP_UNCONFIRMED, str(e))
                if self._moving:
                    self._fail(unconfirmed)
                    raise unconfirmed from e
                raise
            self._moving = False

    def recenter(self) -> None:
        with self._lock:
            self._require_connected()
            self._cancel_deadline()
            self._moving = False
            try:
                self._send_recenter()
            except GimbalError as e:
                self._fail(e)
                raise

    # --- Noi bo ---
    def _require_connected(self) -> None:
        if self._state is not ConnectionState.CONNECTED:
            raise GimbalError(ErrorCode.NOT_CONNECTED, self._state.value)

    def _fail(self, error: GimbalError) -> None:
        self._last_error = error
        # NOT_READY la tam thoi: lien ket van con, khong day sang FAULT.
        if error.code is not ErrorCode.NOT_READY:
            self._state = ConnectionState.FAULT
        log.error("%s", error)

    def _arm_deadline(self, duration_ms: int) -> None:
        self._cancel_deadline()
        self._move_generation += 1
        timer = threading.Timer(duration_ms / 1000.0, self._on_deadline,
                                args=(self._move_generation,))
        timer.daemon = True
        self._deadline_timer = timer
        timer.start()

    def _cancel_deadline(self) -> None:
        self._move_generation += 1
        if self._deadline_timer is not None:
            self._deadline_timer.cancel()
            self._deadline_timer = None

    def _on_deadline(self, generation: int) -> None:
        with self._lock:
            if generation != self._move_generation:
                return
            log.info("het han lenh chuyen dong, tu dung")
            try:
                self.stop()
            except GimbalError:
                pass  # da ghi vao last_error, trang thai FAULT

"""Adapter goi rs3mini-bridge qua HTTP; bridge giu ket noi BLE, khong phai driver nay."""
import json
import time
import urllib.error
import urllib.request
from typing import Optional

from .base import (Capabilities, ConnectionState, ErrorCode, GimbalDriver, GimbalError,
                   GimbalState)


class BridgeHttpDriver(GimbalDriver):
    def __init__(self, base_url: str = "http://127.0.0.1:8787",
                 token: Optional[str] = None, http_timeout_s: float = 1.0,
                 center_timeout_s: float = 10.0, poll_s: float = 0.5):
        super().__init__()
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.http_timeout_s = http_timeout_s
        self.center_timeout_s = center_timeout_s
        self.poll_s = poll_s

    def get_capabilities(self) -> Capabilities:
        # verified_on_hardware chi doi sau khi G01 chay tren RS 3 Mini cua nhom.
        return Capabilities(telemetry=True, angle_reference="relative")

    def get_gimbal_state(self) -> GimbalState:
        data = self._request("GET", "/state")
        telemetry = data.get("telemetry") or {}
        stale = telemetry.get("stale", True)
        return GimbalState(
            moving=bool((data.get("move") or {}).get("active")),
            pan_deg=None if stale else telemetry.get("panDeg"),
            tilt_deg=None if stale else telemetry.get("tiltDeg"),
            roll_deg=None if stale else telemetry.get("rollDeg"),
            battery_percent=telemetry.get("batteryPercent"),
            motor=(data.get("motor") or {}).get("state", "UNKNOWN"),
            telemetry_stale=stale,
        )

    def poll_link(self) -> ConnectionState:
        """Goi dinh ky de phat hien mat BLE; bridge tu reconnect nen CONNECTED co the quay lai."""
        with self._lock:
            label = self._request("GET", "/health/label")
            if self._state is ConnectionState.DISCONNECTED:
                return self._state
            if label == "OK":
                self._state = ConnectionState.CONNECTED
            elif self._state is ConnectionState.CONNECTED:
                self._fail(GimbalError(ErrorCode.NOT_CONNECTED, f"bridge: {label}"))
            return self._state

    def _open(self, timeout_s: float) -> None:
        deadline = time.monotonic() + timeout_s
        last = None
        while time.monotonic() < deadline:
            try:
                last = self._request("GET", "/health/label")
            except GimbalError as e:
                last = str(e)
            if last == "OK":
                return
            time.sleep(self.poll_s)
        raise GimbalError(ErrorCode.TIMEOUT, f"bridge chua noi gimbal: {last}")

    def _close(self) -> None:
        pass  # BLE thuoc ve tien trinh bridge

    def _send_move(self, pan_dir: int, tilt_dir: int) -> None:
        body = {"pan": pan_dir} if pan_dir else {"tilt": tilt_dir}
        self._request("POST", "/move/hold", body)

    def _send_stop(self) -> None:
        self._request("POST", "/move/stop", {})

    def _send_recenter(self) -> None:
        self._request("POST", "/center", {}, timeout_s=self.center_timeout_s)

    def _request(self, method: str, path: str, body: Optional[dict] = None,
                 timeout_s: Optional[float] = None):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.base_url + path, data=data, method=method)
        req.add_header("Content-Type", "application/json")
        if self.token:
            req.add_header("X-Bridge-Token", self.token)
        try:
            with urllib.request.urlopen(req, timeout=timeout_s or self.http_timeout_s) as r:
                return json.loads(r.read().decode() or "null")
        except urllib.error.HTTPError as e:
            raise GimbalError(self._map_status(e.code), f"{method} {path}: HTTP {e.code}") from e
        except TimeoutError as e:
            raise GimbalError(ErrorCode.TIMEOUT, f"{method} {path}") from e
        except urllib.error.URLError as e:
            if isinstance(e.reason, TimeoutError):
                raise GimbalError(ErrorCode.TIMEOUT, f"{method} {path}") from e
            raise GimbalError(ErrorCode.BACKEND, f"{method} {path}: {e.reason}") from e
        except ValueError as e:
            raise GimbalError(ErrorCode.BACKEND, f"{method} {path}: JSON loi") from e

    @staticmethod
    def _map_status(status: int) -> ErrorCode:
        return {400: ErrorCode.INVALID_ARGUMENT, 401: ErrorCode.AUTH,
                503: ErrorCode.NOT_READY}.get(status, ErrorCode.BACKEND)

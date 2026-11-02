"""HTTP service cho module khac goi; giu route /gimbal/* cua app Windows/Android."""
import hmac
import json
import logging
from dataclasses import asdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Optional
from urllib.parse import parse_qs, urlparse

from .base import ErrorCode, GimbalDriver, GimbalError, MAX_MOVE_DURATION_MS
from .monitor import LinkMonitor

log = logging.getLogger("gimbal_driver.service")

HTTP_STATUS = {
    ErrorCode.INVALID_ARGUMENT: 400,
    ErrorCode.UNSUPPORTED: 501,
    ErrorCode.NOT_CONNECTED: 503,
    ErrorCode.NOT_READY: 503,
    ErrorCode.STOP_UNCONFIRMED: 503,
    ErrorCode.TIMEOUT: 504,
}


def _direction(params: dict, name: str) -> int:
    try:
        value = float(params.get(name, ["0"])[0])
    except ValueError:
        raise GimbalError(ErrorCode.INVALID_ARGUMENT, f"{name} khong hop le")
    return (value > 0) - (value < 0)


def _duration_ms(params: dict) -> int:
    try:
        return int(params.get("duration_ms", [str(MAX_MOVE_DURATION_MS)])[0])
    except ValueError:
        raise GimbalError(ErrorCode.INVALID_ARGUMENT, "duration_ms khong hop le")


def build_status(driver: GimbalDriver) -> dict:
    error = driver.get_last_error()
    status = {"connection": driver.get_connection_state().value,
              "last_error": error.code.value if error else None}
    try:
        status["gimbal"] = asdict(driver.get_gimbal_state())
    except GimbalError as e:
        status["gimbal"] = None
        status["gimbal_error"] = e.code.value
    return status


def make_handler(driver: GimbalDriver, monitor: Optional[LinkMonitor], token: Optional[str]):
    class Handler(BaseHTTPRequestHandler):
        def _reply(self, status: int, payload) -> None:
            raw = json.dumps(payload).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def _authorized(self, params: dict) -> bool:
            if not token:
                return True
            given = self.headers.get("X-Gimbal-Token") or params.get("token", [""])[0]
            return hmac.compare_digest(given.encode(), token.encode())

        def _route(self, path: str, params: dict):
            if path == "/gimbal/move":
                pan, tilt = _direction(params, "pan"), _direction(params, "tilt")
                if pan == 0 and tilt == 0:
                    driver.stop()
                else:
                    driver.move(pan, tilt, _duration_ms(params))
                return {"gimbal": "move", "pan": pan, "tilt": tilt}
            if path == "/gimbal/stop":
                driver.stop()
                return {"gimbal": "stop"}
            if path == "/gimbal/recenter":
                driver.recenter()
                return {"gimbal": "recenter"}
            if path == "/gimbal/mode":
                raise GimbalError(ErrorCode.UNSUPPORTED, "chua co lenh doi che do da kiem chung")
            if path == "/gimbal/status":
                return build_status(driver)
            if path == "/gimbal/capabilities":
                return asdict(driver.get_capabilities())
            if path == "/gimbal/connect":
                if monitor is not None:
                    monitor.reset()
                driver.connect(float(params.get("timeout_s", ["60"])[0]))
                return {"gimbal": "connected"}
            if path == "/gimbal/disconnect":
                driver.disconnect()
                return {"gimbal": "disconnected"}
            return None

        def _handle(self) -> None:
            url = urlparse(self.path)
            params = parse_qs(url.query)
            if not self._authorized(params):
                self._reply(401, {"error": "AUTH"})
                return
            try:
                result = self._route(url.path, params)
            except GimbalError as e:
                self._reply(HTTP_STATUS.get(e.code, 502),
                            {"error": e.code.value, "message": str(e)})
                return
            except ValueError:
                self._reply(400, {"error": ErrorCode.INVALID_ARGUMENT.value})
                return
            if result is None:
                self._reply(404, {"error": "NOT_FOUND"})
            else:
                self._reply(200, result)

        do_GET = _handle  # app Windows/Android hien goi bang GET
        do_POST = _handle

        def log_message(self, fmt, *args):
            log.info("%s %s", self.address_string(), fmt % args)

    return Handler


def make_server(driver: GimbalDriver, host: str, port: int, monitor: Optional[LinkMonitor] = None,
                token: Optional[str] = None) -> ThreadingHTTPServer:
    return ThreadingHTTPServer((host, port), make_handler(driver, monitor, token))

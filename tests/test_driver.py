import json
import sys
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import (BridgeHttpDriver, ConnectionState, ErrorCode, GimbalError,
                           MockGimbalDriver)


class MockDriverTest(unittest.TestCase):
    def setUp(self):
        self.d = MockGimbalDriver()
        self.d.connect()

    def tearDown(self):
        self.d._cancel_deadline()

    def assert_code(self, code, fn, *args):
        with self.assertRaises(GimbalError) as ctx:
            fn(*args)
        self.assertIs(ctx.exception.code, code)

    def test_capabilities_do_not_overclaim(self):
        caps = self.d.get_capabilities()
        self.assertFalse(caps.speed_control)
        self.assertFalse(caps.absolute_angle)
        self.assertFalse(caps.verified_on_hardware)

    def test_invalid_moves_rejected(self):
        self.assert_code(ErrorCode.INVALID_ARGUMENT, self.d.move, 2, 0, 100)
        self.assert_code(ErrorCode.INVALID_ARGUMENT, self.d.move, 0, 0, 100)
        self.assert_code(ErrorCode.INVALID_ARGUMENT, self.d.move, 1, 0, 0)
        self.assert_code(ErrorCode.INVALID_ARGUMENT, self.d.move, 1, 0, 999999)
        self.assert_code(ErrorCode.UNSUPPORTED, self.d.move, 1, 1, 100)

    def test_deadline_stops_motion(self):
        self.d.move(1, 0, 50)
        self.assertTrue(self.d.get_gimbal_state().moving)
        time.sleep(0.2)
        self.assertFalse(self.d.get_gimbal_state().moving)

    def test_refresh_extends_deadline(self):
        self.d.move(1, 0, 120)
        time.sleep(0.07)
        self.d.move(1, 0, 120)
        time.sleep(0.07)
        self.assertTrue(self.d.get_gimbal_state().moving)

    def test_stop_cancels_deadline(self):
        self.d.move(1, 0, 50)
        self.d.stop()
        self.d.move(0, 1, 500)
        time.sleep(0.1)
        self.assertTrue(self.d.get_gimbal_state().moving)

    def test_link_loss_reports_stop_unconfirmed(self):
        self.d.move(1, 0, 50)
        self.d.drop_link()
        time.sleep(0.2)
        self.assertTrue(self.d.physically_moving)
        self.assertIs(self.d.get_connection_state(), ConnectionState.FAULT)
        self.assertIs(self.d.get_last_error().code, ErrorCode.STOP_UNCONFIRMED)

    def test_reconnect_clears_fault(self):
        self.d.move(1, 0, 50)
        self.d.drop_link()
        time.sleep(0.2)
        self.d.restore_link()
        self.d.connect()
        self.assertIs(self.d.get_connection_state(), ConnectionState.CONNECTED)
        self.d.stop()
        self.assertFalse(self.d.physically_moving)

    def test_duplicate_move_is_throttled(self):
        for _ in range(5):
            self.d.move(1, 0, 1000)
        self.assertEqual(self.d.commands.count(("move", 1, 0)), 1)

    def test_reconnect_sends_stop_for_interrupted_move(self):
        self.d.move(1, 0, 2000)
        self.d.drop_link()
        self.d.check_link()
        self.d.restore_link()
        self.d.connect()
        self.assertFalse(self.d.physically_moving)

    def test_move_requires_connection(self):
        self.d.disconnect()
        self.assert_code(ErrorCode.NOT_CONNECTED, self.d.move, 1, 0, 100)


class FakeBridge(BaseHTTPRequestHandler):
    calls = []
    move_status = 200
    label = "OK"
    state = {"move": {"active": False}, "motor": {"state": "AWAKE"},
             "telemetry": {"panDeg": 1.5, "tiltDeg": 2.5, "rollDeg": 0, "stale": False,
                           "batteryPercent": 57}}

    def _reply(self, status, payload):
        raw = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        self._reply(200, FakeBridge.label if self.path == "/health/label" else FakeBridge.state)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length) or b"{}")
        FakeBridge.calls.append((self.path, body, self.headers.get("X-Bridge-Token")))
        status = FakeBridge.move_status if self.path == "/move/hold" else 200
        self._reply(status, {"ok": status == 200})

    def log_message(self, *args):
        pass


class BridgeDriverTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = HTTPServer(("127.0.0.1", 0), FakeBridge)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def setUp(self):
        FakeBridge.calls = []
        FakeBridge.move_status = 200
        FakeBridge.label = "OK"
        self.d = BridgeHttpDriver(f"http://127.0.0.1:{self.server.server_port}",
                                  token="t", poll_s=0.01)
        self.d.connect(timeout_s=1)

    def tearDown(self):
        self.d._cancel_deadline()

    def test_move_stop_and_center_routes(self):
        self.d.move(-1, 0, 1000)
        self.d.move(0, 1, 1000)
        self.d.stop()
        self.d.recenter()
        self.assertEqual([(p, b) for p, b, _ in FakeBridge.calls], [
            ("/move/hold", {"pan": -1}), ("/move/hold", {"tilt": 1}),
            ("/move/stop", {}), ("/center", {})])
        self.assertEqual(FakeBridge.calls[0][2], "t")

    def test_state_parsing(self):
        state = self.d.get_gimbal_state()
        self.assertEqual((state.pan_deg, state.tilt_deg, state.battery_percent), (1.5, 2.5, 57))

    def test_stale_telemetry_is_unknown(self):
        original = FakeBridge.state
        FakeBridge.state = {**original, "telemetry": {"panDeg": 9, "stale": True}}
        try:
            self.assertIsNone(self.d.get_gimbal_state().pan_deg)
        finally:
            FakeBridge.state = original

    def test_guard_503_keeps_link_connected(self):
        FakeBridge.move_status = 503
        with self.assertRaises(GimbalError) as ctx:
            self.d.move(1, 0, 100)
        self.assertIs(ctx.exception.code, ErrorCode.NOT_READY)
        self.assertIs(self.d.get_connection_state(), ConnectionState.CONNECTED)

    def test_poll_detects_lost_ble_then_recovery(self):
        FakeBridge.label = "NO GIMB"
        self.assertIs(self.d.check_link(), ConnectionState.FAULT)
        FakeBridge.label = "OK"
        self.d.connect(timeout_s=1)
        self.assertIs(self.d.check_link(), ConnectionState.CONNECTED)

    def test_connect_timeout(self):
        FakeBridge.label = "CONN…"
        with self.assertRaises(GimbalError) as ctx:
            self.d.connect(timeout_s=0.05)
        self.assertIs(ctx.exception.code, ErrorCode.TIMEOUT)


if __name__ == "__main__":
    unittest.main()

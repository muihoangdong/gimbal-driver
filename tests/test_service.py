import json
import sys
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gimbal_driver import ConnectionState, LinkMonitor, MockGimbalDriver
from gimbal_driver.__main__ import load_config
from gimbal_driver.service import make_server


class ServiceTest(unittest.TestCase):
    def setUp(self):
        self.d = MockGimbalDriver()
        self.d.connect()
        self.server = make_server(self.d, "127.0.0.1", 0, token="secret")
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def tearDown(self):
        self.d._cancel_deadline()
        self.server.shutdown()
        self.server.server_close()

    def get(self, path, token="secret"):
        req = urllib.request.Request(f"http://127.0.0.1:{self.server.server_port}{path}")
        if token:
            req.add_header("X-Gimbal-Token", token)
        try:
            with urllib.request.urlopen(req, timeout=2) as r:
                return r.status, json.loads(r.read())
        except urllib.error.HTTPError as e:
            return e.code, json.loads(e.read())

    def test_rejects_missing_token(self):
        self.assertEqual(self.get("/gimbal/status", token=None)[0], 401)

    def test_windows_app_move_then_release(self):
        self.assertEqual(self.get("/gimbal/move?pan=1&tilt=0")[0], 200)
        self.assertTrue(self.d.physically_moving)
        self.assertEqual(self.get("/gimbal/move?pan=0&tilt=0")[0], 200)
        self.assertFalse(self.d.physically_moving)

    def test_two_axes_unsupported(self):
        status, body = self.get("/gimbal/move?pan=1&tilt=1")
        self.assertEqual((status, body["error"]), (501, "UNSUPPORTED"))

    def test_mode_is_not_claimed(self):
        self.assertEqual(self.get("/gimbal/mode?m=PF")[0], 501)

    def test_status_and_capabilities(self):
        self.assertEqual(self.get("/gimbal/status")[1]["connection"], "CONNECTED")
        self.assertFalse(self.get("/gimbal/capabilities")[1]["absolute_angle"])

    def test_bad_input_and_unknown_route(self):
        self.assertEqual(self.get("/gimbal/move?pan=abc")[0], 400)
        self.assertEqual(self.get("/nope")[0], 404)


class MonitorTest(unittest.TestCase):
    def wait_for(self, condition):
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            if condition():
                return True
            time.sleep(0.01)
        return False

    def make(self, attempts):
        d = MockGimbalDriver()
        d.connect()
        events = []
        m = LinkMonitor(d, interval_s=0.01, max_attempts=attempts, base_backoff_s=0.01,
                        on_event=lambda name, detail: events.append(name))
        m.start()
        self.addCleanup(m.stop)
        return d, m, events

    def test_reconnects_after_link_returns(self):
        d, _, events = self.make(50)
        d.drop_link()
        self.assertTrue(self.wait_for(lambda: "LINK_LOST" in events))
        d.restore_link()
        self.assertTrue(self.wait_for(lambda: "RECONNECTED" in events))
        self.assertIs(d.get_connection_state(), ConnectionState.CONNECTED)

    def test_gives_up_after_limit(self):
        d, m, events = self.make(2)
        d.drop_link()
        self.assertTrue(self.wait_for(lambda: "GAVE_UP" in events))
        self.assertTrue(m.gave_up)


class ConfigTest(unittest.TestCase):
    def test_lan_requires_token(self):
        path = Path(__file__).with_name("_tmp_config.json")
        path.write_text('{"host": "0.0.0.0"}', encoding="utf-8")
        self.addCleanup(path.unlink)
        with self.assertRaises(ValueError):
            load_config(str(path))


if __name__ == "__main__":
    unittest.main()

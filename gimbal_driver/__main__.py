"""Chay service: python -m gimbal_driver --config config.json"""
import argparse
import json
import logging
import logging.handlers
import sys

from .base import GimbalError
from .bridge_http import BridgeHttpDriver
from .mock import MockGimbalDriver
from .monitor import LinkMonitor
from .service import make_server

DEFAULTS = {
    "driver": "mock",
    "bridge_url": "http://127.0.0.1:8787",
    "bridge_token": None,
    "host": "127.0.0.1",
    "port": 8080,
    "token": None,
    "log_file": None,
    "connect_timeout_s": 60,
    "monitor_interval_s": 1.0,
    "reconnect_attempts": 5,
}


def load_config(path):
    config = dict(DEFAULTS)
    if path:
        with open(path, encoding="utf-8") as f:
            config.update(json.load(f))
    unknown = set(config) - set(DEFAULTS)
    if unknown:
        raise ValueError(f"khoa cau hinh la: {sorted(unknown)}")
    if config["host"] not in ("127.0.0.1", "localhost") and not config["token"]:
        raise ValueError("mo service ra mang thi phai dat token")
    return config


def setup_logging(log_file):
    handlers = [logging.StreamHandler()]
    if log_file:
        handlers.append(logging.handlers.RotatingFileHandler(
            log_file, maxBytes=5_000_000, backupCount=3, encoding="utf-8"))
    logging.basicConfig(level=logging.INFO, handlers=handlers,
                        format="%(asctime)s %(levelname)s %(name)s %(message)s")


def build_driver(config):
    if config["driver"] == "mock":
        return MockGimbalDriver()
    if config["driver"] == "bridge":
        return BridgeHttpDriver(config["bridge_url"], token=config["bridge_token"])
    raise ValueError(f"driver khong ho tro: {config['driver']}")


def main():
    parser = argparse.ArgumentParser(description="Gimbal Driver service")
    parser.add_argument("--config")
    args = parser.parse_args()
    try:
        config = load_config(args.config)
        driver = build_driver(config)
    except (OSError, ValueError) as e:
        sys.exit(f"Loi cau hinh: {e}")
    setup_logging(config["log_file"])
    log = logging.getLogger("gimbal_driver")

    try:
        driver.connect(config["connect_timeout_s"])
    except GimbalError as e:
        log.error("chua ket noi duoc, monitor se thu lai: %s", e)

    monitor = LinkMonitor(driver, interval_s=config["monitor_interval_s"],
                          max_attempts=config["reconnect_attempts"],
                          connect_timeout_s=config["connect_timeout_s"])
    monitor.start()
    server = make_server(driver, config["host"], config["port"], monitor, config["token"])
    log.info("service chay tai %s:%s voi driver %s", config["host"], config["port"],
             config["driver"])
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        monitor.stop()
        server.server_close()
        driver.disconnect()


if __name__ == "__main__":
    main()

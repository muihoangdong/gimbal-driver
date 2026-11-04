#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "gimbal_driver/gimbal.hpp"

using namespace gimbal;
using json = nlohmann::json;

namespace {

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

template <typename Fn>
ErrorCode errorCodeOf(Fn&& fn) {
    try {
        fn();
    } catch (const GimbalError& e) {
        return e.code();
    }
    ADD_FAILURE() << "khong nem GimbalError";
    return ErrorCode::Backend;
}

}  // namespace

class MockDriverTest : public ::testing::Test {
protected:
    void SetUp() override { d.connect(); }
    MockGimbalDriver d;
};

TEST_F(MockDriverTest, CapabilitiesDoNotOverclaim) {
    const Capabilities caps = d.getCapabilities();
    EXPECT_FALSE(caps.speed_control);
    EXPECT_FALSE(caps.absolute_angle);
    EXPECT_FALSE(caps.verified_on_hardware);
}

TEST_F(MockDriverTest, InvalidMovesRejected) {
    EXPECT_EQ(errorCodeOf([&] { d.move(2, 0, 100); }), ErrorCode::InvalidArgument);
    EXPECT_EQ(errorCodeOf([&] { d.move(0, 0, 100); }), ErrorCode::InvalidArgument);
    EXPECT_EQ(errorCodeOf([&] { d.move(1, 0, 0); }), ErrorCode::InvalidArgument);
    EXPECT_EQ(errorCodeOf([&] { d.move(1, 0, 999999); }), ErrorCode::InvalidArgument);
    EXPECT_EQ(errorCodeOf([&] { d.move(1, 1, 100); }), ErrorCode::Unsupported);
}

TEST_F(MockDriverTest, DeadlineStopsMotion) {
    d.move(1, 0, 50);
    EXPECT_TRUE(d.getGimbalState().moving);
    sleepMs(200);
    EXPECT_FALSE(d.getGimbalState().moving);
}

TEST_F(MockDriverTest, RefreshExtendsDeadline) {
    d.move(1, 0, 120);
    sleepMs(70);
    d.move(1, 0, 120);
    sleepMs(70);
    EXPECT_TRUE(d.getGimbalState().moving);
}

TEST_F(MockDriverTest, StopCancelsDeadline) {
    d.move(1, 0, 50);
    d.stop();
    d.move(0, 1, 500);
    sleepMs(100);
    EXPECT_TRUE(d.getGimbalState().moving);
}

TEST_F(MockDriverTest, LinkLossReportsStopUnconfirmed) {
    d.move(1, 0, 50);
    d.dropLink();
    sleepMs(200);
    EXPECT_TRUE(d.physicallyMoving());
    EXPECT_EQ(d.getConnectionState(), ConnectionState::Fault);
    ASSERT_TRUE(d.getLastError());
    EXPECT_EQ(d.getLastError()->code(), ErrorCode::StopUnconfirmed);
}

TEST_F(MockDriverTest, ReconnectClearsFault) {
    d.move(1, 0, 50);
    d.dropLink();
    sleepMs(200);
    d.restoreLink();
    d.connect();
    EXPECT_EQ(d.getConnectionState(), ConnectionState::Connected);
    d.stop();
    EXPECT_FALSE(d.physicallyMoving());
}

TEST_F(MockDriverTest, DuplicateMoveIsThrottled) {
    for (int i = 0; i < 5; ++i) d.move(1, 0, 1000);
    const auto commands = d.commands();
    EXPECT_EQ(std::count(commands.begin(), commands.end(), "move 1 0"), 1);
}

TEST_F(MockDriverTest, ReconnectSendsStopForInterruptedMove) {
    d.move(1, 0, 2000);
    d.dropLink();
    d.checkLink();
    d.restoreLink();
    d.connect();
    EXPECT_FALSE(d.physicallyMoving());
}

TEST_F(MockDriverTest, MoveRequiresConnection) {
    d.disconnect();
    EXPECT_EQ(errorCodeOf([&] { d.move(1, 0, 100); }), ErrorCode::NotConnected);
}

// Bridge gia lap tra loi giong API cua rs3mini-bridge.
class FakeBridge {
public:
    FakeBridge() {
        server_.Get(".*", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(mutex_);
            const json body = req.path == "/health/label" ? json(label) : state;
            res.set_content(body.dump(), "application/json");
        });
        server_.Post(".*", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(mutex_);
            calls.push_back({req.path, req.body.empty() ? json::object() : json::parse(req.body),
                             req.get_header_value("X-Bridge-Token")});
            res.status = req.path == "/move/hold" ? move_status : 200;
            res.set_content(json{{"ok", res.status == 200}}.dump(), "application/json");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~FakeBridge() {
        server_.stop();
        thread_.join();
    }

    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    struct Call {
        std::string path;
        json body;
        std::string token;
    };

    std::mutex mutex_;
    std::vector<Call> calls;
    int move_status = 200;
    std::string label = "OK";
    json state = {{"move", {{"active", false}}},
                  {"ble", {{"connected", true}, {"rssiDbm", -55}}},
                  {"motor", {{"state", "AWAKE"}}},
                  {"telemetry", {{"panDeg", 1.5}, {"tiltDeg", 2.5}, {"rollDeg", 0},
                                 {"stale", false}, {"batteryPercent", 57}}}};

private:
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
};

class BridgeDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        BridgeHttpOptions options;
        options.base_url = bridge.url();
        options.token = "t";
        options.poll_s = 0.01;
        d = std::make_unique<BridgeHttpDriver>(options);
        d->connect(1.0);
    }
    void setBridge(std::function<void(FakeBridge&)> change) {
        std::lock_guard<std::mutex> lock(bridge.mutex_);
        change(bridge);
    }

    FakeBridge bridge;
    std::unique_ptr<BridgeHttpDriver> d;
};

TEST_F(BridgeDriverTest, MoveStopAndCenterRoutes) {
    d->move(-1, 0, 1000);
    d->move(0, 1, 1000);
    d->stop();
    d->recenter();
    std::lock_guard<std::mutex> lock(bridge.mutex_);
    ASSERT_EQ(bridge.calls.size(), 4u);
    EXPECT_EQ(bridge.calls[0].path, "/move/hold");
    EXPECT_EQ(bridge.calls[0].body, (json{{"pan", -1}}));
    EXPECT_EQ(bridge.calls[1].body, (json{{"tilt", 1}}));
    EXPECT_EQ(bridge.calls[2].path, "/move/stop");
    EXPECT_EQ(bridge.calls[3].path, "/center");
    EXPECT_EQ(bridge.calls[0].token, "t");
}

TEST_F(BridgeDriverTest, StateParsing) {
    const GimbalState state = d->getGimbalState();
    EXPECT_EQ(state.pan_deg, 1.5);
    EXPECT_EQ(state.tilt_deg, 2.5);
    EXPECT_EQ(state.battery_percent, 57);
    EXPECT_EQ(state.motor, "AWAKE");
    EXPECT_EQ(state.link_rssi_dbm, -55);
}

TEST_F(BridgeDriverTest, StaleTelemetryIsUnknown) {
    setBridge([](FakeBridge& b) { b.state["telemetry"] = {{"panDeg", 9}, {"stale", true}}; });
    EXPECT_FALSE(d->getGimbalState().pan_deg.has_value());
}

TEST_F(BridgeDriverTest, Guard503KeepsLinkConnected) {
    setBridge([](FakeBridge& b) { b.move_status = 503; });
    EXPECT_EQ(errorCodeOf([&] { d->move(1, 0, 100); }), ErrorCode::NotReady);
    EXPECT_EQ(d->getConnectionState(), ConnectionState::Connected);
}

TEST_F(BridgeDriverTest, PollDetectsLostBleThenRecovery) {
    setBridge([](FakeBridge& b) { b.label = "NO GIMB"; });
    EXPECT_EQ(d->checkLink(), ConnectionState::Fault);
    setBridge([](FakeBridge& b) { b.label = "OK"; });
    d->connect(1.0);
    EXPECT_EQ(d->checkLink(), ConnectionState::Connected);
}

TEST_F(BridgeDriverTest, ConnectTimeout) {
    setBridge([](FakeBridge& b) { b.label = "CONN..."; });
    EXPECT_EQ(errorCodeOf([&] { d->connect(0.05); }), ErrorCode::Timeout);
}

TEST_F(BridgeDriverTest, NullFieldsWhileGimbalDropsAreTolerated) {
    // Gap tren phan cung: bridge tra null khi gimbal vua rot BLE.
    setBridge([](FakeBridge& b) {
        b.state = {{"move", {{"active", nullptr}}},
                   {"motor", {{"state", nullptr}}},
                   {"ble", {{"connected", false}, {"rssiDbm", nullptr}}},
                   {"telemetry", {{"stale", nullptr}, {"panDeg", nullptr}, {"batteryPercent", nullptr}}}};
    });
    const GimbalState state = d->getGimbalState();
    EXPECT_TRUE(state.telemetry_stale);
    EXPECT_FALSE(state.moving);
    EXPECT_EQ(state.motor, "UNKNOWN");
    EXPECT_FALSE(state.link_rssi_dbm.has_value());
    EXPECT_FALSE(state.battery_percent.has_value());
}

TEST_F(BridgeDriverTest, StateStaysResponsiveWhileReconnecting) {
    // Gap tren phan cung: connect() cho bridge toi 60 s lam /gimbal/status treo.
    setBridge([](FakeBridge& b) { b.label = "CONN..."; });
    std::thread connecting([&] { errorCodeOf([&] { d->connect(1.5); }); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto started = std::chrono::steady_clock::now();
    EXPECT_EQ(d->getConnectionState(), ConnectionState::Connecting);
    d->getLastError();
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(100));
    connecting.join();
    EXPECT_EQ(d->getConnectionState(), ConnectionState::Fault);
}

TEST(BridgeDriverOffline, UnreachableBridgeIsBackendOrTimeout) {
    BridgeHttpOptions options;
    options.base_url = "http://127.0.0.1:1";
    options.http_timeout_s = 0.3;
    BridgeHttpDriver d(options);
    const ErrorCode code = errorCodeOf([&] { d.getGimbalState(); });
    EXPECT_TRUE(code == ErrorCode::Backend || code == ErrorCode::Timeout);
}

TEST_F(MockDriverTest, ReferenceZerosAnglesUntilCleared) {
    d.move(1, 0, 200);
    sleepMs(300);
    const double raw_pan = *d.getGimbalState().pan_deg;
    ASSERT_GT(raw_pan, 1.0);
    d.setReference();
    GimbalState state = d.getGimbalState();
    EXPECT_TRUE(state.reference_set);
    EXPECT_NEAR(*state.pan_deg, 0.0, 1e-6);
    d.clearReference();
    state = d.getGimbalState();
    EXPECT_FALSE(state.reference_set);
    EXPECT_NEAR(*state.pan_deg, raw_pan, 1e-6);
}

TEST_F(BridgeDriverTest, ReferenceWrapsAroundPlusMinus180) {
    setBridge([](FakeBridge& b) { b.state["telemetry"]["panDeg"] = 170.0; });
    d->setReference();
    setBridge([](FakeBridge& b) { b.state["telemetry"]["panDeg"] = -170.0; });
    EXPECT_NEAR(*d->getGimbalState().pan_deg, 20.0, 1e-6);
}

TEST_F(BridgeDriverTest, ReferenceNeedsFreshTelemetry) {
    setBridge([](FakeBridge& b) { b.state["telemetry"]["stale"] = true; });
    EXPECT_EQ(errorCodeOf([&] { d->setReference(); }), ErrorCode::NotReady);
}

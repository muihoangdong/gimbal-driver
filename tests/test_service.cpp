#include <algorithm>
#include <memory>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "gimbal_driver/config.hpp"
#include "gimbal_driver/gimbal.hpp"
#include "gimbal_driver/service.hpp"

using namespace gimbal;
using json = nlohmann::json;

class ServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        d.connect();
        port = service.bind("127.0.0.1", 0);
        ASSERT_GT(port, 0);
        thread = std::thread([this] { service.listen(); });
        service.waitUntilReady();
    }
    void TearDown() override {
        service.stop();
        thread.join();
    }

    std::pair<int, json> get(const std::string& path, const std::string& token = "secret") {
        httplib::Client client("127.0.0.1", port);
        httplib::Headers headers;
        if (!token.empty()) headers.emplace("X-Gimbal-Token", token);
        auto res = client.Get(path, headers);
        if (!res) return {0, json()};
        return {res->status, json::parse(res->body)};
    }

    MockGimbalDriver d;
    GimbalService service{d, nullptr, std::string("secret")};
    int port = 0;
    std::thread thread;
};

TEST_F(ServiceTest, RejectsMissingToken) { EXPECT_EQ(get("/gimbal/status", "").first, 401); }

TEST_F(ServiceTest, WindowsAppMoveThenRelease) {
    EXPECT_EQ(get("/gimbal/move?pan=1&tilt=0").first, 200);
    EXPECT_TRUE(d.physicallyMoving());
    EXPECT_EQ(get("/gimbal/move?pan=0&tilt=0").first, 200);
    EXPECT_FALSE(d.physicallyMoving());
}

TEST_F(ServiceTest, TwoAxesUnsupported) {
    const auto [status, body] = get("/gimbal/move?pan=1&tilt=1");
    EXPECT_EQ(status, 501);
    EXPECT_EQ(body["error"], "UNSUPPORTED");
}

TEST_F(ServiceTest, ModeIsNotClaimed) { EXPECT_EQ(get("/gimbal/mode?m=PF").first, 501); }

TEST_F(ServiceTest, StatusAndCapabilities) {
    EXPECT_EQ(get("/gimbal/status").second["connection"], "CONNECTED");
    EXPECT_EQ(get("/gimbal/capabilities").second["absolute_angle"], false);
}

TEST_F(ServiceTest, BadInputAndUnknownRoute) {
    EXPECT_EQ(get("/gimbal/move?pan=abc").first, 400);
    EXPECT_EQ(get("/nope").first, 404);
}

class MonitorTest : public ::testing::Test {
protected:
    template <typename Pred>
    bool waitFor(Pred condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
    bool sawEvent(const std::string& name) {
        std::lock_guard<std::mutex> lock(events_mutex);
        return std::find(events.begin(), events.end(), name) != events.end();
    }
    std::unique_ptr<LinkMonitor> make(int attempts) {
        d.connect();
        LinkMonitorOptions options;
        options.interval_s = 0.01;
        options.max_attempts = attempts;
        options.base_backoff_s = 0.01;
        auto monitor = std::make_unique<LinkMonitor>(
            d, options, [this](const std::string& name, const std::string&) {
                std::lock_guard<std::mutex> lock(events_mutex);
                events.push_back(name);
            });
        monitor->start();
        return monitor;
    }

    MockGimbalDriver d;
    std::mutex events_mutex;
    std::vector<std::string> events;
};

TEST_F(MonitorTest, ReconnectsAfterLinkReturns) {
    auto monitor = make(50);
    d.dropLink();
    EXPECT_TRUE(waitFor([&] { return sawEvent("LINK_LOST"); }));
    d.restoreLink();
    EXPECT_TRUE(waitFor([&] { return sawEvent("RECONNECTED"); }));
    EXPECT_EQ(d.getConnectionState(), ConnectionState::Connected);
}

TEST_F(MonitorTest, GivesUpAfterLimit) {
    auto monitor = make(2);
    d.dropLink();
    EXPECT_TRUE(waitFor([&] { return sawEvent("GAVE_UP"); }));
    EXPECT_TRUE(monitor->gaveUp());
}

TEST(ConfigTest, LanRequiresToken) {
    const std::string path = "_tmp_config.json";
    std::ofstream(path) << R"({"host": "0.0.0.0"})";
    EXPECT_THROW(loadConfig(path), std::invalid_argument);
    std::remove(path.c_str());
}

TEST(ConfigTest, UnknownKeyRejected) {
    const std::string path = "_tmp_config2.json";
    std::ofstream(path) << R"({"prot": 8080})";
    EXPECT_THROW(loadConfig(path), std::invalid_argument);
    std::remove(path.c_str());
}

TEST(ConfigTest, DefaultsAreMockOnLocalhost) {
    const Config config = loadConfig(std::nullopt);
    EXPECT_EQ(config.driver, "mock");
    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 8080);
}

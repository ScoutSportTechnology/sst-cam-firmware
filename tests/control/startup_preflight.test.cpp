#include <gtest/gtest.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "adapters/control/network/net-iface.hpp"
#include "adapters/control/system/camera-probe.hpp"
#include "adapters/control/system/systemd-notify.hpp"
#include "app/control/services/startup-preflight.hpp"

namespace {
using sst::control::StartupPreflight;
using sst::control::StartupProbes;
using sst_cam::COMPONENT_STATUS_MISSING;
using sst_cam::COMPONENT_STATUS_OK;

struct Fixture {
    StartupProbes probes{.camera0 = sst_cam::CAMERA_HEALTH_OK,
                         .camera1 = sst_cam::CAMERA_HEALTH_OK,
                         .gpu = COMPONENT_STATUS_OK,
                         .wifi = COMPONENT_STATUS_OK,
                         .ethernet = COMPONENT_STATUS_OK,
                         .bluetooth = COMPONENT_STATUS_OK,
                         .storage = COMPONENT_STATUS_OK,
                         .clock_synced = true};
    int ble = 0;
    int network = 0;
    int http = 0;
    int pipeline = 0;
    bool ble_starts = true;
    bool ble_running = false;
    bool pipeline_starts = true;
    bool network_starts = true;
    auto Make() -> StartupPreflight {
        return StartupPreflight([this] { return probes; }, {.ble =
                                                                [this] {
                                                                    if (!ble_running) {
                                                                        ++ble;
                                                                        ble_running = ble_starts;
                                                                    }
                                                                    return ble_running;
                                                                },
                                                            .network =
                                                                [this] {
                                                                    ++network;
                                                                    return network_starts;
                                                                },
                                                            .http =
                                                                [this] {
                                                                    ++http;
                                                                    return true;
                                                                },
                                                            .pipeline =
                                                                [this] {
                                                                    ++pipeline;
                                                                    return pipeline_starts;
                                                                }});
    }
};

TEST(UplinkHealthTest, MissingCarrierAndAddressAreNotOk) {
    using sst::adapters::control::UplinkHealth;
    EXPECT_EQ(UplinkHealth("", true, true), COMPONENT_STATUS_MISSING);
    EXPECT_EQ(UplinkHealth("enP8p1s0", false, true), sst_cam::COMPONENT_STATUS_DEGRADED);
    EXPECT_EQ(UplinkHealth("enP8p1s0", true, false), sst_cam::COMPONENT_STATUS_DEGRADED);
    EXPECT_EQ(UplinkHealth("enP8p1s0", true, true), COMPONENT_STATUS_OK);
}

TEST(CameraProbeTest, ChildOutputDoesNotDetermineHealth) {
    using sst::adapters::control::RunCameraProbeChild;
    constexpr auto timeout = std::chrono::seconds{1};
    const std::vector<std::string> noisy{"sh", "-c", "printf 'GST_ARGUS: capture\\n'; exit 0"};
    EXPECT_EQ(RunCameraProbeChild(noisy, timeout), sst_cam::CAMERA_HEALTH_OK);
    EXPECT_EQ(RunCameraProbeChild({"sh", "-c", "exit 10"}, timeout), sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(RunCameraProbeChild({"sh", "-c", "exit 42"}, timeout), sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(RunCameraProbeChild({"sh", "-c", "kill -TERM $$"}, timeout),
              sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(RunCameraProbeChild({"sleep", "30"}, timeout), sst_cam::CAMERA_HEALTH_DOWN);
}

TEST(SystemdNotifyTest, MissingSocketSucceedsAndClosedSocketFails) {
    const char* saved = std::getenv("NOTIFY_SOCKET");
    const std::string previous = saved == nullptr ? "" : saved;
    ::unsetenv("NOTIFY_SOCKET");
    EXPECT_TRUE(sst::adapters::control::NotifySystemd("READY=1"));
    const std::string socket_path = "/tmp/sst-notify-" + std::to_string(::getpid()) + ".sock";
    ::setenv("NOTIFY_SOCKET", socket_path.c_str(), 1);
    EXPECT_FALSE(sst::adapters::control::NotifySystemd("READY=1"));
    if (saved == nullptr) {
        ::unsetenv("NOTIFY_SOCKET");
    } else {
        ::setenv("NOTIFY_SOCKET", previous.c_str(), 1);
    }
}

TEST(StartupPreflightTest, MissingGpuKeepsControlReadyAndRetriesCaptureAfterRecovery) {
    Fixture fixture;
    fixture.probes.gpu = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.pipeline, 0);
    EXPECT_EQ(startup.Components().gpu(), COMPONENT_STATUS_MISSING);
    fixture.probes.gpu = COMPONENT_STATUS_OK;
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 1);
    EXPECT_EQ(startup.Components().gpu(), COMPONENT_STATUS_OK);
}

TEST(StartupPreflightTest, MissingCameraDoesNotStartCaptureButKeepsControlReady) {
    Fixture fixture;
    fixture.probes.camera1 = sst_cam::CAMERA_HEALTH_DOWN;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.pipeline, 0);
    EXPECT_EQ(startup.CameraHealth(1), sst_cam::CAMERA_HEALTH_DOWN);
    fixture.probes.camera1 = sst_cam::CAMERA_HEALTH_OK;
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 1);
}

TEST(StartupPreflightTest, CameraZeroMissingNeverStartsCapture) {
    Fixture fixture;
    fixture.probes.camera0 = sst_cam::CAMERA_HEALTH_DOWN;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 0);
    EXPECT_TRUE(startup.Ready());
}

TEST(StartupPreflightTest, ProbeChildCrashOrTimeoutNeverStartsCapture) {
    for (const auto failure : {sst_cam::CAMERA_HEALTH_DOWN, sst_cam::CAMERA_HEALTH_UNKNOWN}) {
        Fixture fixture;
        fixture.probes.camera0 = failure;
        auto startup = fixture.Make();
        startup.Poll();
        EXPECT_EQ(fixture.pipeline, 0);
        EXPECT_EQ(startup.CameraHealth(0), failure);
    }
}

TEST(StartupPreflightTest, MissingBluetoothKeepsReadyAndRetriesBle) {
    Fixture fixture;
    fixture.probes.bluetooth = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 1);
    EXPECT_EQ(startup.Components().bluetooth(), COMPONENT_STATUS_OK);
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    fixture.ble_running = false;
    fixture.probes.bluetooth = COMPONENT_STATUS_OK;
    fixture.ble_starts = false;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    fixture.ble_starts = true;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 3);
}

TEST(StartupPreflightTest, MissingHci0StillAttemptsBleAndReportsRegistration) {
    Fixture fixture;
    fixture.probes.bluetooth = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(fixture.ble, 1);
    EXPECT_EQ(startup.Components().bluetooth(), COMPONENT_STATUS_OK);
}

TEST(StartupPreflightTest, BluetoothHealthTracksRegistrationAndRetriesAfterLoss) {
    Fixture fixture;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(startup.Components().bluetooth(), COMPONENT_STATUS_OK);
    fixture.ble_running = false;
    fixture.ble_starts = false;
    startup.Poll();
    EXPECT_EQ(startup.Components().bluetooth(), sst_cam::COMPONENT_STATUS_DEGRADED);
    EXPECT_EQ(fixture.ble, 2);
    fixture.ble_starts = true;
    startup.Poll();
    EXPECT_EQ(startup.Components().bluetooth(), COMPONENT_STATUS_OK);
    EXPECT_EQ(fixture.ble, 3);
}

TEST(StartupPreflightTest, PresentAdapterWithoutSuccessfulRegistrationIsNotHealthy) {
    Fixture fixture;
    fixture.ble_starts = false;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(startup.Components().bluetooth(), sst_cam::COMPONENT_STATUS_DEGRADED);
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 1);
}

TEST(StartupPreflightTest, ComponentsRemainsReadableWhileBleRegistrationWaits) {
    Fixture fixture;
    std::promise<void> entered;
    std::promise<void> release;
    auto resume = release.get_future().share();
    int ble_calls = 0;
    auto startup =
        StartupPreflight([&] { return fixture.probes; }, {.ble =
                                                              [&] {
                                                                  if (++ble_calls == 2) {
                                                                      entered.set_value();
                                                                      resume.wait();
                                                                  }
                                                                  return true;
                                                              },
                                                          .network = [] { return true; },
                                                          .http = [] { return true; },
                                                          .pipeline = [] { return true; }});
    startup.Poll();
    std::thread polling([&] { startup.Poll(); });
    const auto entered_status = entered.get_future().wait_for(std::chrono::seconds{3});
    EXPECT_EQ(entered_status, std::future_status::ready);
    if (entered_status == std::future_status::ready) {
        auto components = std::async(std::launch::async, [&] { return startup.Components(); });
        const auto read_status = components.wait_for(std::chrono::milliseconds{200});
        release.set_value();
        EXPECT_EQ(read_status, std::future_status::ready);
        EXPECT_EQ(components.get().bluetooth(), COMPONENT_STATUS_OK);
    } else {
        release.set_value();
    }
    polling.join();
}

TEST(StartupPreflightTest, ConcurrentPollsDoNotOverlapActions) {
    Fixture fixture;
    std::promise<void> entered;
    std::promise<void> release;
    auto resume = release.get_future().share();
    std::atomic<int> ble_calls{0};
    auto startup =
        StartupPreflight([&] { return fixture.probes; }, {.ble =
                                                              [&] {
                                                                  if (++ble_calls == 1) {
                                                                      entered.set_value();
                                                                      resume.wait();
                                                                  }
                                                                  return true;
                                                              },
                                                          .network = [] { return true; },
                                                          .http = [] { return true; },
                                                          .pipeline = [] { return true; }});
    std::thread first([&] { startup.Poll(); });
    const auto entered_status = entered.get_future().wait_for(std::chrono::seconds{3});
    EXPECT_EQ(entered_status, std::future_status::ready);
    std::thread second([&] { startup.Poll(); });
    if (entered_status == std::future_status::ready) {
        constexpr auto kOverlapWait = std::chrono::milliseconds{200};
        std::this_thread::sleep_for(kOverlapWait);
        EXPECT_EQ(ble_calls.load(), 1);
    }
    release.set_value();
    first.join();
    second.join();
    EXPECT_EQ(ble_calls.load(), 2);
    EXPECT_TRUE(startup.Ready());
}

TEST(StartupPreflightTest, PeriodicPollWaitIsInterruptibleBeforeBleAction) {
    Fixture fixture;
    std::atomic<int> polls{0};
    auto startup =
        StartupPreflight([&] { return fixture.probes; }, {.ble =
                                                              [&] {
                                                                  ++polls;
                                                                  return true;
                                                              },
                                                          .network = [] { return true; },
                                                          .http = [] { return true; },
                                                          .pipeline = [] { return true; }});
    std::jthread periodic([&](const std::stop_token& stop) {
        constexpr auto kRetryInterval = std::chrono::seconds{30};
        startup.RunPeriodically(stop, kRetryInterval, [] {});
    });
    const auto start = std::chrono::steady_clock::now();
    periodic.request_stop();
    periodic.join();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{3});
    EXPECT_EQ(polls.load(), 0);
}

TEST(StartupPreflightTest, StoppingPeriodicPollWaitsForInFlightBleAction) {
    Fixture fixture;
    std::promise<void> entered;
    std::promise<void> release;
    auto resume = release.get_future().share();
    auto startup =
        StartupPreflight([&] { return fixture.probes; }, {.ble =
                                                              [&] {
                                                                  entered.set_value();
                                                                  resume.wait();
                                                                  return true;
                                                              },
                                                          .network = [] { return true; },
                                                          .http = [] { return true; },
                                                          .pipeline = [] { return true; }});
    std::jthread periodic([&](const std::stop_token& stop) {
        startup.RunPeriodically(stop, std::chrono::milliseconds{1}, [] {});
    });
    const auto entered_status = entered.get_future().wait_for(std::chrono::seconds{3});
    EXPECT_EQ(entered_status, std::future_status::ready);
    periodic.request_stop();
    auto joined = std::async(std::launch::async, [&] { periodic.join(); });
    const auto join_status = joined.wait_for(std::chrono::milliseconds{200});
    release.set_value();
    EXPECT_EQ(join_status, std::future_status::timeout);
    EXPECT_EQ(joined.wait_for(std::chrono::seconds{3}), std::future_status::ready);
    joined.get();
}

TEST(StartupPreflightTest, PeriodicPollDoesNotOccupyCallingWatchdogThread) {
    Fixture fixture;
    std::promise<void> entered;
    std::promise<void> release;
    auto resume = release.get_future().share();
    auto startup =
        StartupPreflight([&] { return fixture.probes; }, {.ble =
                                                              [&] {
                                                                  entered.set_value();
                                                                  resume.wait();
                                                                  return true;
                                                              },
                                                          .network = [] { return true; },
                                                          .http = [] { return true; },
                                                          .pipeline = [] { return true; }});
    std::jthread periodic([&](const std::stop_token& stop) {
        startup.RunPeriodically(stop, std::chrono::milliseconds{1}, [] {});
    });
    const auto entered_status = entered.get_future().wait_for(std::chrono::seconds{3});
    EXPECT_EQ(entered_status, std::future_status::ready);
    auto heartbeat = std::async(std::launch::async, [] { return true; });
    const auto heartbeat_status = heartbeat.wait_for(std::chrono::milliseconds{200});
    release.set_value();
    EXPECT_EQ(heartbeat_status, std::future_status::ready);
    EXPECT_TRUE(heartbeat.get());
    periodic.request_stop();
    periodic.join();
}

TEST(StartupPreflightTest, ReadyNotificationIsClaimedExactlyOnce) {
    Fixture fixture;
    auto startup = fixture.Make();
    EXPECT_FALSE(startup.TakeReadyNotification());
    startup.Poll();
    EXPECT_TRUE(startup.TakeReadyNotification());
    EXPECT_FALSE(startup.TakeReadyNotification());
    startup.Poll();
    EXPECT_FALSE(startup.TakeReadyNotification());
}

TEST(StartupPreflightTest, AllPresentStartsServicesOnceInDependencyOrder) {
    Fixture fixture;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 1);
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    EXPECT_EQ(fixture.pipeline, 1);
    startup.Poll();
    EXPECT_EQ(fixture.ble, 1);
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    EXPECT_EQ(fixture.pipeline, 1);
    EXPECT_TRUE(startup.Components().clock_synced());
}

TEST(StartupPreflightTest, FailedCaptureReportsDownAndRetries) {
    Fixture fixture;
    fixture.pipeline_starts = false;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(startup.CameraHealth(0), sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(startup.CameraHealth(1), sst_cam::CAMERA_HEALTH_DOWN);
    fixture.pipeline_starts = true;
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 2);
}

TEST(StartupPreflightTest, UnknownCameraNeverStartsCapture) {
    Fixture fixture;
    fixture.probes.camera0 = sst_cam::CAMERA_HEALTH_UNKNOWN;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 0);
}

TEST(StartupPreflightTest, NoUplinkAddressStillStartsNetworkAndReportsDegraded) {
    Fixture fixture;
    fixture.probes.wifi = sst_cam::COMPONENT_STATUS_DEGRADED;
    fixture.probes.ethernet = sst_cam::COMPONENT_STATUS_DEGRADED;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(startup.Components().wifi(), sst_cam::COMPONENT_STATUS_DEGRADED);
    EXPECT_EQ(startup.Components().ethernet(), sst_cam::COMPONENT_STATUS_DEGRADED);
}

TEST(StartupPreflightTest, NetworkStartFailureKeepsHttpDownButSendsReadyAndRetries) {
    Fixture fixture;
    fixture.network_starts = false;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.http, 0);
    fixture.network_starts = true;
    startup.Poll();
    EXPECT_EQ(fixture.network, 2);
    EXPECT_EQ(fixture.http, 1);
    EXPECT_TRUE(startup.Ready());
}

TEST(StartupPreflightTest, MissingUplinksStillStartNetworkingAndHttp) {
    Fixture fixture;
    fixture.probes.wifi = COMPONENT_STATUS_MISSING;
    fixture.probes.ethernet = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    fixture.probes.ethernet = COMPONENT_STATUS_OK;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
}
}  // namespace

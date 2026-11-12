#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

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
    bool pipeline_starts = true;
    bool network_starts = true;
    auto Make() -> StartupPreflight {
        return StartupPreflight([this] { return probes; }, {.ble =
                                                                [this] {
                                                                    ++ble;
                                                                    return ble_starts;
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

TEST(CameraProbeTest, ChildCrashTimeoutAndBadOutputFailClosed) {
    using sst::adapters::control::DecodeCameraProbe;
    EXPECT_EQ(DecodeCameraProbe("1 1\n", true)[0], sst_cam::CAMERA_HEALTH_OK);
    EXPECT_EQ(DecodeCameraProbe("1 0\n", true)[1], sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(DecodeCameraProbe("1 1\n", false)[0], sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(DecodeCameraProbe("1 1\n", false)[1], sst_cam::CAMERA_HEALTH_DOWN);
    EXPECT_EQ(DecodeCameraProbe("1 1 junk", true)[0], sst_cam::CAMERA_HEALTH_DOWN);
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
    EXPECT_EQ(fixture.ble, 0);
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    fixture.probes.bluetooth = COMPONENT_STATUS_OK;
    fixture.ble_starts = false;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    fixture.ble_starts = true;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 2);
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

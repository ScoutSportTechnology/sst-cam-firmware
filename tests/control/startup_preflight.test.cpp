#include <gtest/gtest.h>

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
    auto Make() -> StartupPreflight {
        return StartupPreflight([this] { return probes; }, {.ble =
                                                                [this] {
                                                                    ++ble;
                                                                    return ble_starts;
                                                                },
                                                            .network =
                                                                [this] {
                                                                    ++network;
                                                                    return true;
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
    EXPECT_EQ(fixture.pipeline, 1);
    EXPECT_EQ(startup.CameraHealth(1), sst_cam::CAMERA_HEALTH_DOWN);
    fixture.probes.camera1 = sst_cam::CAMERA_HEALTH_UNKNOWN;
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 1);
}

TEST(StartupPreflightTest, MissingBluetoothWithholdsReadyUntilItRecovers) {
    Fixture fixture;
    fixture.probes.bluetooth = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_FALSE(startup.Ready());
    EXPECT_EQ(fixture.ble, 0);
    EXPECT_EQ(fixture.network, 1);
    EXPECT_EQ(fixture.http, 1);
    fixture.probes.bluetooth = COMPONENT_STATUS_OK;
    fixture.ble_starts = false;
    startup.Poll();
    EXPECT_FALSE(startup.Ready());
    fixture.ble_starts = true;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.ble, 2);
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

TEST(StartupPreflightTest, UnknownCameraNeedsARealCaptureAttempt) {
    Fixture fixture;
    fixture.probes.camera0 = sst_cam::CAMERA_HEALTH_UNKNOWN;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_EQ(fixture.pipeline, 1);
}

TEST(StartupPreflightTest, NoNetworkKeepsHttpDownAndReadyWithheld) {
    Fixture fixture;
    fixture.probes.wifi = COMPONENT_STATUS_MISSING;
    fixture.probes.ethernet = COMPONENT_STATUS_MISSING;
    auto startup = fixture.Make();
    startup.Poll();
    EXPECT_FALSE(startup.Ready());
    EXPECT_EQ(fixture.network, 0);
    EXPECT_EQ(fixture.http, 0);
    fixture.probes.ethernet = COMPONENT_STATUS_OK;
    startup.Poll();
    EXPECT_TRUE(startup.Ready());
    EXPECT_EQ(fixture.http, 1);
}
}  // namespace

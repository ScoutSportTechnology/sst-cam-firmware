set(_hardware_tests
    RawCaptureSinkE2E.StartPushStopWritesBothProxyFiles
    RawCaptureSinkE2E.StartWhileCapturingIsRejected
    RawCaptureSinkE2E.StopIsIdempotentlySafe
    GstreamerAdapter.CaptureSingleFrameAndLog
    BleAdvertisementTest.AdvertisesOnRealBluez
    WpaWifiManagerE2E.FormsAutonomousGroupOwner
    StreamingE2E.AppStreamServerStartsAndAcceptsPushes
    StreamingE2E.RtmpStreamerAcceptsCompleteUrlWithoutSeparateKey
    StreamingE2E.RtmpStreamerStartsAndPushesAgainstLocalIngest
    GstContinuousE2E.EncodesSinglePlayableMp4
    GstOverlayE2E.AppSrcAcceptsRgbaPush
)
foreach(_test IN LISTS _hardware_tests)
    list(FIND sst_cam_firmware_tests_TESTS "${_test}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR "Missing hardware test: ${_test}")
    endif()
    set_tests_properties("${_test}" PROPERTIES LABELS hardware)
endforeach()
set_tests_properties(StreamingE2E.AppStreamServerStartsAndAcceptsPushes PROPERTIES RESOURCE_LOCK rtsp_port_18554)
set_tests_properties(StreamingE2E.RtmpStreamerStartsAndPushesAgainstLocalIngest PROPERTIES RESOURCE_LOCK rtmp_port_1935)

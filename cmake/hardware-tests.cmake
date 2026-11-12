execute_process(
    COMMAND ${SST_TEST_EMULATOR} "${SST_TEST_BINARY}" --gtest_list_tests
    OUTPUT_VARIABLE _listed
    RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Failed to list GoogleTest cases: ${_result}")
endif()

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
    GstOverlayE2E.AppSrcAcceptsRgbaPush)

string(REPLACE "\n" ";" _lines "${_listed}")
set(_suite "")
set(_cases "")
set(_found_hardware "")
set(_all_cases "")
foreach(_line IN LISTS _lines)
    if(_line MATCHES "^([^ #].*[.])($|[ ]*#)")
        set(_suite "${CMAKE_MATCH_1}")
        string(REGEX REPLACE "\\.$" "" _suite "${_suite}")
    elseif(_line MATCHES "^  ([^ #]+)")
        if(_suite STREQUAL "")
            message(FATAL_ERROR "GoogleTest case without suite: ${_line}")
        endif()
        set(_case "${_suite}.${CMAKE_MATCH_1}")
        list(APPEND _all_cases "${_case}")
        list(FIND _hardware_tests "${_case}" _index)
        if(NOT _index EQUAL -1)
            list(APPEND _found_hardware "${_case}")
        endif()
        if(NOT DEFINED ENV{SST_TEST_FILTER} OR "$ENV{SST_TEST_FILTER}" STREQUAL "" OR
           _case MATCHES "$ENV{SST_TEST_FILTER}")
            list(APPEND _cases "${_case}")
        endif()
    endif()
endforeach()
list(LENGTH _all_cases _case_count)
if(_case_count EQUAL 0)
    message(FATAL_ERROR "No GoogleTest cases discovered")
endif()
message("GoogleTest cases discovered: ${_case_count}")
foreach(_hardware IN LISTS _hardware_tests)
    list(FIND _found_hardware "${_hardware}" _index)
    if(_index EQUAL -1)
        message(FATAL_ERROR "Missing hardware test: ${_hardware}")
    endif()
endforeach()

set(_suites "")
foreach(_case IN LISTS _cases)
    list(FIND _hardware_tests "${_case}" _index)
    if(NOT _index EQUAL -1)
        add_test("${_case}" ${SST_TEST_EMULATOR} "${SST_TEST_BINARY}"
            "--gtest_filter=${_case}")
        set_tests_properties("${_case}" PROPERTIES LABELS hardware)
    else()
        string(REGEX REPLACE "\\.[^.]+$" "" _suite "${_case}")
        list(APPEND _suites "${_suite}")
        string(MAKE_C_IDENTIFIER "${_suite}" _id)
        list(APPEND _suite_cases_${_id} "${_case}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _suites)
foreach(_suite IN LISTS _suites)
    string(MAKE_C_IDENTIFIER "${_suite}" _id)
    string(JOIN ":" _filter ${_suite_cases_${_id}})
    add_test("${_suite}" ${SST_TEST_EMULATOR} "${SST_TEST_BINARY}"
        "--gtest_filter=${_filter}")
endforeach()
set_tests_properties(StreamingE2E.AppStreamServerStartsAndAcceptsPushes PROPERTIES RESOURCE_LOCK rtsp_port_18554)
set_tests_properties(StreamingE2E.RtmpStreamerStartsAndPushesAgainstLocalIngest PROPERTIES RESOURCE_LOCK rtmp_port_1935)

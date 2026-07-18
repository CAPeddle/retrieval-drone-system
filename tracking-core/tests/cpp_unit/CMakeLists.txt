# ─── tracking_core_tests: links tracking_core_lib instead of raw sources ─────
add_executable(tracking_core_tests
    test_ball_detector.cpp
    test_calibration_marker_detector.cpp
    test_capture_thread.cpp
    test_config.cpp
    test_frame_metadata.cpp
    test_frame_quality.cpp
    test_frame_ring_buffer.cpp
    test_replay_integration.cpp
    test_replay_source.cpp
    test_logging.cpp
    test_logging_default_level.cpp
    test_track.cpp
    test_tracker.cpp
)

target_include_directories(tracking_core_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/core
)

target_link_libraries(tracking_core_tests
    PRIVATE
        tracking_core_lib
        GTest::gtest_main
)

# Absolute path to the shipped config template, so the template-load smoke test
# can verify it against the real loader regardless of the ctest working dir.
target_compile_definitions(tracking_core_tests
    PRIVATE
        TRACKING_CORE_CONFIG_TEMPLATE="${CMAKE_SOURCE_DIR}/config/tracking_core.yaml"
)

include(GoogleTest)
gtest_discover_tests(tracking_core_tests)

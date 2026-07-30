# ─── tracking_core_tests: links tracking_core_lib instead of raw sources ─────
add_executable(tracking_core_tests
    test_ball_detector.cpp
    test_calibration_marker_detector.cpp
    test_capture_thread.cpp
    test_config.cpp
    test_frame_metadata.cpp
    test_frame_quality.cpp
    test_frame_ring_buffer.cpp
    test_coordinate_mapper.cpp
    test_homography_loader.cpp
    test_replay_integration.cpp
    test_replay_source.cpp
    test_logging.cpp
    test_logging_default_level.cpp
    test_modulation_detector.cpp
    test_modulation_fixture.cpp
    test_pipeline_tracking.cpp
    test_safe_for_control.cpp
    test_safety_replay.cpp
    test_snapshot_builder.cpp
    test_spsc_ring.cpp
    test_track.cpp
    test_tracker.cpp
    test_zmq_publisher.cpp
)

target_include_directories(tracking_core_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/core
)

target_link_libraries(tracking_core_tests
    PRIVATE
        tracking_core_lib
        GTest::gtest_main
        # Calibration fixtures build the TRK-012/013 JSON artifacts directly.
        nlohmann_json::nlohmann_json
        # test_zmq_publisher round-trips through a SUB socket.
        cppzmq
)

# Absolute path to the shipped config template, so the template-load smoke test
# can verify it against the real loader regardless of the ctest working dir.
target_compile_definitions(tracking_core_tests
    PRIVATE
        TRACKING_CORE_CONFIG_TEMPLATE="${CMAKE_SOURCE_DIR}/config/tracking_core.yaml"
)

include(GoogleTest)
gtest_discover_tests(tracking_core_tests)

# ─── tracking_core_tests: links tracking_core_lib instead of raw sources ─────
add_executable(tracking_core_tests
    test_config.cpp
    test_logging.cpp
    test_logging_default_level.cpp
    test_tracking_pipeline.cpp
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

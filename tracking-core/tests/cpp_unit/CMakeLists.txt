# ─── tracking_core_tests: links tracking_core_lib instead of raw sources ─────
add_executable(tracking_core_tests
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

include(GoogleTest)
gtest_discover_tests(tracking_core_tests)

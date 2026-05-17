add_executable(tracking_core_tests
    test_tracking_pipeline.cpp
    ../../src/core/tracking/detector.cpp
    ../../src/core/tracking/tracker.cpp
)

target_include_directories(tracking_core_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/core
        ${PROJECT_SOURCE_DIR}/src/core/include
)

target_link_libraries(tracking_core_tests
    PRIVATE
        GTest::gtest_main
        opencv_core
)

include(GoogleTest)
gtest_discover_tests(tracking_core_tests)

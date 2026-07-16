# ─── tracking_core_lib: static library of all pipeline sources ────────────────
# Used by both the main executable and unit tests. Excludes main.cpp.
add_library(tracking_core_lib STATIC
    config.cpp
    tracking/detector.cpp
    tracking/tracker.cpp
)

target_include_directories(tracking_core_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tracking_core_lib
    PUBLIC
        ${OpenCV_LIBS}
        # PUBLIC: logging.hpp exposes spdlog macros to every consumer TU — that
        # is what makes compile-time gating and format-arg elision zero-cost.
        spdlog::spdlog
    PRIVATE
        cppzmq
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
)

# ─── tracking_core: main executable ──────────────────────────────────────────
add_executable(tracking_core
    main.cpp
)

target_include_directories(tracking_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tracking_core
    PRIVATE
        tracking_core_lib
        cppzmq
)

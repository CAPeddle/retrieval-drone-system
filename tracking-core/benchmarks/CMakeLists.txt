# ─── psd_strategy_bench: standalone KTD-4 measurement binary (TRK-009a) ───────
# NOT a unit test — never registered with ctest. It links tracking_core_lib (for
# OpenCV + the shared include dirs) and reaches the header-only modulation
# fixture through an include path to tests/cpp_unit. Run manually on the Pi 5 in
# Release; its stdout (timing + correctness table) is the deliverable, and the
# executor applies the KTD-4 rule to the on-target numbers.
add_executable(psd_strategy_bench
    psd_strategy_bench.cpp
)

target_link_libraries(psd_strategy_bench
    PRIVATE
        tracking_core_lib
)

target_include_directories(psd_strategy_bench
    PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/cpp_unit
)

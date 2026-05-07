add_executable(tracking_core
    main.cpp
    tracking/detector.cpp
    tracking/tracker.cpp
)

target_include_directories(tracking_core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tracking_core
    PRIVATE
        opencv_core
        opencv_imgproc
        opencv_videoio
        opencv_imgcodecs
        zmq
)

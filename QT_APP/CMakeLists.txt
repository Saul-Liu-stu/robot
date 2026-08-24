cmake_minimum_required(VERSION 3.16)
project(RobotBleApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Bluetooth Network)

qt_add_executable(RobotBleApp
    main.cpp
    protocol_core.h      protocol_core.cpp
    bluetoothclient.h    bluetoothclient.cpp
    servowidget.h        servowidget.cpp
    pidwidget.h          pidwidget.cpp
    motorwidget.h        motorwidget.cpp
    joystickwidget.h     joystickwidget.cpp
    camerawidget.h       camerawidget.cpp
    color_detector.h     color_detector.cpp
    motion_detector.h    motion_detector.cpp
    light_detector.h     light_detector.cpp
    gimbaldial.h         gimbaldial.cpp
    splashscreen.h       splashscreen.cpp
    mainwindow.h         mainwindow.cpp
)

# v6.17 logo 资源: Qt 6.11 的 qt_add_resources 在 Android 下走 assets 部署 (依赖 androiddeployqt),
# 本项目构建流程跳过 androiddeployqt, 故改用 rcc 直接把 qrc 编译进 .so
set(QRC_OUT ${CMAKE_CURRENT_BINARY_DIR}/qrc_resources.cpp)
add_custom_command(OUTPUT ${QRC_OUT}
    COMMAND $<TARGET_FILE:Qt6::rcc> --name resources --output ${QRC_OUT}
            ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png
    VERBATIM)
set_source_files_properties(${QRC_OUT} PROPERTIES GENERATED TRUE)
target_sources(RobotBleApp PRIVATE ${QRC_OUT})

target_link_libraries(RobotBleApp PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Bluetooth
    Qt6::Network
)

if(ANDROID)
    set_target_properties(RobotBleApp PROPERTIES
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
    target_link_libraries(RobotBleApp PRIVATE log)
endif()

cmake_minimum_required(VERSION 3.16)
project(RobotBleApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Bluetooth)

qt_add_executable(RobotBleApp
    main.cpp
    protocol_core.h      protocol_core.cpp
    bluetoothclient.h    bluetoothclient.cpp
    servowidget.h        servowidget.cpp
    pidwidget.h          pidwidget.cpp
    motorwidget.h        motorwidget.cpp
    joystickwidget.h     joystickwidget.cpp
    mainwindow.h         mainwindow.cpp
)

target_link_libraries(RobotBleApp PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Bluetooth
)

if(ANDROID)
    set_target_properties(RobotBleApp PROPERTIES
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
    target_link_libraries(RobotBleApp PRIVATE log)
endif()

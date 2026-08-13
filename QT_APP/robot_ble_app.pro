# ============================================================
# 四足机器人蓝牙调试助手 - Qt Project File
# 协议: HC-05 SPP (经典蓝牙 RFCOMM)
# 目标: Android (ARM) / Windows Desktop
# Qt 版本: 6.11.1
# ============================================================

QT += core gui widgets bluetooth

TARGET = RobotBleApp
TEMPLATE = app

CONFIG += c++17

# ---------- 源文件 ----------
SOURCES += \
    main.cpp \
    protocol_core.cpp \
    bluetoothclient.cpp \
    motorwidget.cpp \
    controlwidget.cpp \
    mainwindow.cpp

HEADERS += \
    protocol_core.h \
    bluetoothclient.h \
    motorwidget.h \
    controlwidget.h \
    mainwindow.h

# ---------- Android 配置 ----------
android {
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    # 目标 ABI (必须至少选一个)
    ANDROID_ABIS = arm64-v8a

    ANDROID_TARGET_SDK_VERSION = 35
    ANDROID_MIN_SDK_VERSION = 26
    ANDROID_COMPILE_SDK_VERSION = 35

    DISTFILES += \
        android/AndroidManifest.xml \
        android/res/values/strings.xml

    # Qt 6 JNI 在 QtCore 中，无需额外模块
}

# Windows 桌面配置
win32 {
    CONFIG += console
    RC_ICONS = icon.ico
}

# 安全编码
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

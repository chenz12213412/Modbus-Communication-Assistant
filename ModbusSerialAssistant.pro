QT += core gui widgets serialport network

CONFIG += c++17
TEMPLATE = app
TARGET = ModbusSerialAssistant

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

RESOURCES += \
    resources.qrc

RC_FILE = app_icon.rc

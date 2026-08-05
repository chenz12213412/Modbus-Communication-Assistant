QT += core gui widgets serialport network

CONFIG += c++17
TEMPLATE = app
TARGET = ModbusSerialAssistant
VERSION = 1.0.4

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    internalchannel.cpp \
    communicationstatistics.cpp \
    slavedatasimulator.cpp \
    trendchart.cpp

HEADERS += \
    mainwindow.h \
    internalchannel.h \
    communicationstatistics.h \
    slavedatasimulator.h \
    trendchart.h

RESOURCES += \
    resources.qrc

RC_FILE = app_icon.rc

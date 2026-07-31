cmake_minimum_required(VERSION 3.16)

project(ModbusSerialAssistant VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Network)

add_executable(ModbusSerialAssistant WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    resources.qrc
    app_icon.rc
)

target_link_libraries(ModbusSerialAssistant PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    target_compile_definitions(ModbusSerialAssistant PRIVATE NOMINMAX)
endif()

install(TARGETS ModbusSerialAssistant
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

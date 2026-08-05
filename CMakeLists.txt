cmake_minimum_required(VERSION 3.16)

project(ModbusSerialAssistant VERSION 1.0.4 LANGUAGES CXX)

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
    internalchannel.cpp
    internalchannel.h
    communicationstatistics.cpp
    communicationstatistics.h
    slavedatasimulator.cpp
    slavedatasimulator.h
    trendchart.cpp
    trendchart.h
    resources.qrc
    app_icon.rc
)

include(CTest)
if(BUILD_TESTING)
    add_executable(internalchannel_test
        tests/internalchannel_test.cpp
        internalchannel.cpp
        internalchannel.h
    )
    target_include_directories(internalchannel_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(internalchannel_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME internalchannel_test COMMAND internalchannel_test)

    add_executable(slavedatasimulator_test
        tests/slavedatasimulator_test.cpp
        slavedatasimulator.cpp
        slavedatasimulator.h
    )
    target_include_directories(slavedatasimulator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(slavedatasimulator_test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME slavedatasimulator_test COMMAND slavedatasimulator_test)

    add_executable(communicationstatistics_test
        tests/communicationstatistics_test.cpp
        communicationstatistics.cpp
        communicationstatistics.h
    )
    target_include_directories(communicationstatistics_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(communicationstatistics_test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
    add_test(NAME communicationstatistics_test COMMAND communicationstatistics_test)

    add_executable(mainwindow_update_test
        tests/mainwindow_update_test.cpp
        mainwindow.cpp
        mainwindow.h
        internalchannel.cpp
        internalchannel.h
        communicationstatistics.cpp
        communicationstatistics.h
        slavedatasimulator.cpp
        slavedatasimulator.h
        trendchart.cpp
        trendchart.h
        resources.qrc
    )
    target_include_directories(mainwindow_update_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(mainwindow_update_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::SerialPort
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME mainwindow_update_test COMMAND mainwindow_update_test)
    set_tests_properties(mainwindow_update_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

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

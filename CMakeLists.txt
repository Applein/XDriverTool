cmake_minimum_required(VERSION 3.16)

project(XDriverTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

add_executable(XDriverTool
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/timeseriesplot.cpp
    src/timeseriesplot.h
    src/xdrvstatus.cpp
    src/xdrvstatus.h
)

target_link_libraries(XDriverTool PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

if(WIN32)
    set(_qt_package_dir_variable "Qt${QT_VERSION_MAJOR}_DIR")
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${${_qt_package_dir_variable}}/../../../bin"
    )
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found; required to deploy the Windows runtime.")
    endif()

    add_custom_command(TARGET XDriverTool POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --compiler-runtime
            --no-translations
            "$<TARGET_FILE:XDriverTool>"
        COMMENT "Deploying Qt runtime beside XDriverTool"
        VERBATIM
    )
endif()

if(MSVC)
    target_compile_options(XDriverTool PRIVATE /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(XDriverToolStatusTests
        tests/test_xdrvstatus.cpp
        src/xdrvstatus.cpp
        src/xdrvstatus.h
    )
    target_include_directories(XDriverToolStatusTests PRIVATE src)
    target_link_libraries(XDriverToolStatusTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Test
    )
    if(MSVC)
        target_compile_options(XDriverToolStatusTests PRIVATE /utf-8)
    endif()
    add_test(NAME XDriverToolStatusTests COMMAND XDriverToolStatusTests)

    add_executable(XDriverToolPlotTests
        tests/test_timeseriesplot.cpp
        src/timeseriesplot.cpp
        src/timeseriesplot.h
    )
    target_include_directories(XDriverToolPlotTests PRIVATE src)
    target_link_libraries(XDriverToolPlotTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
    )
    if(MSVC)
        target_compile_options(XDriverToolPlotTests PRIVATE /utf-8)
    endif()
    add_test(NAME XDriverToolPlotTests COMMAND XDriverToolPlotTests)
endif()

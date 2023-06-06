cmake_minimum_required(VERSION 3.5)
project(cell_monitor)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Widgets REQUIRED)
find_package(Qt5Network REQUIRED)

set(SOURCES
    main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Widgets Qt5::Network)

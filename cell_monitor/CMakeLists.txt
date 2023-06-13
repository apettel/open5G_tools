cmake_minimum_required(VERSION 3.5)
project(cell_monitor)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
message(STATUS "MODULES PATH " ${CMAKE_MODULE_PATH})
find_package(Qt5 COMPONENTS Core Widgets Network REQUIRED)

find_package(SRSGUI)
if(SRSGUI_FOUND)
    include_directories(${SRSGUI_INCLUDE_DIRS})
    link_directories(${SRSGUI_LIBRARY_DIRS})
endif(SRSGUI_FOUND)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBIIO REQUIRED libiio)

set(SOURCES
    main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

SET(CMAKE_CXX_FLAGS_DEBUG "... -lpthread")
SET(CMAKE_CXX_FLAGS_RELEASE "... -lpthread")

target_include_directories(${PROJECT_NAME} PRIVATE ${LIBIIO_INCLUDE_DIRS} ${SRSGUI_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Widgets Qt5::Network ${LIBIIO_LIBRARIES} ${SRSGUI_LIBRARIES})

cmake_minimum_required(VERSION 3.16)
project(FlightSchedulingSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows socket definitions required by cpp-httplib
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

# Allow passing -DMYSQL_CONNECTOR_DIR="C:/Program Files/MySQL/MySQL Connector C++ 8.4"
set(MYSQL_CONNECTOR_DIR "" CACHE PATH "Path to MySQL Connector/C++ installation root")

if(NOT MYSQL_CONNECTOR_DIR)
    if(DEFINED ENV{MYSQL_CONNECTOR_DIR})
        set(MYSQL_CONNECTOR_DIR "$ENV{MYSQL_CONNECTOR_DIR}")
    elseif(WIN32)
        # Common default installation paths on Windows
        file(GLOB POSSIBLE_MYSQL_DIRS "C:/Program Files/MySQL/MySQL Connector C++*")
        if(POSSIBLE_MYSQL_DIRS)
            list(GET POSSIBLE_MYSQL_DIRS 0 MYSQL_CONNECTOR_DIR)
        endif()
    endif()
endif()

message(STATUS "Using MYSQL_CONNECTOR_DIR: ${MYSQL_CONNECTOR_DIR}")

find_path(MYSQLCPPCONN_INCLUDE_DIR
    NAMES mysql_driver.h jdbc/mysql_driver.h
    PATHS
        "${MYSQL_CONNECTOR_DIR}/include"
        "${MYSQL_CONNECTOR_DIR}/include/jdbc"
        "C:/msys64/ucrt64/include"
        "/usr/include"
        "/usr/local/include"
)

find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8 mysqlcppconn9
    PATHS
        "${MYSQL_CONNECTOR_DIR}/lib64/vs14"
        "${MYSQL_CONNECTOR_DIR}/lib64"
        "${MYSQL_CONNECTOR_DIR}/lib"
        "C:/msys64/ucrt64/lib"
        "/usr/lib"
        "/usr/local/lib"
)

if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
    message(FATAL_ERROR
        "Could not locate MySQL Connector/C++ headers or libraries.\n"
        "Pass -DMYSQL_CONNECTOR_DIR=\"C:/Program Files/MySQL/MySQL Connector C++ 8.4\" to CMake."
    )
endif()

message(STATUS "Found MySQL Connector/C++ Include: ${MYSQLCPPCONN_INCLUDE_DIR}")
message(STATUS "Found MySQL Connector/C++ Library: ${MYSQLCPPCONN_LIBRARY}")

set(SOURCES
    backend/src/main.cpp
    backend/src/Config.cpp
    backend/src/Database.cpp
    backend/src/Utils.cpp
    backend/src/ConflictDetector.cpp
    backend/src/FlightScheduler.cpp
    backend/src/ApiRoutes.cpp
)

add_executable(flight_ops_server ${SOURCES})

target_include_directories(flight_ops_server PRIVATE
    "${CMAKE_SOURCE_DIR}/backend/include"
    "${CMAKE_SOURCE_DIR}/backend/third_party"
    "${MYSQLCPPCONN_INCLUDE_DIR}"
    "${MYSQLCPPCONN_INCLUDE_DIR}/jdbc"
)

target_link_libraries(flight_ops_server PRIVATE
    "${MYSQLCPPCONN_LIBRARY}"
)

if(WIN32)
    target_link_libraries(flight_ops_server PRIVATE ws2_32 crypt32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(flight_ops_server PRIVATE Threads::Threads)
endif()

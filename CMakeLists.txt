cmake_minimum_required(VERSION 3.10)
project(UltraFastMapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ultrafast_mapper
    src/main.cpp
    src/Scheduler.cpp
    src/Routing.cpp
    src/Graph.cpp
    src/CGRA.cpp
    src/ReservationTable.cpp
)

target_include_directories(ultrafast_mapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
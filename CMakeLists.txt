cmake_minimum_required(VERSION 4.0)
project(kursach2)

set(CMAKE_CXX_STANDARD 20)

add_executable(kursach2 main.cpp)

target_link_libraries(kursach2 PRIVATE gdiplus)

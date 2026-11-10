# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(plasma_scroller CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(plasma_scroller
    main.cpp
    plasma_effect.cpp
)

target_link_libraries(plasma_scroller
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
)
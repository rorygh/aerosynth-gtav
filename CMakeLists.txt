cmake_minimum_required(VERSION 3.13)
project(aerosynth_gtav LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(aerosynth_gtav SHARED
    src/main.cpp
    src/script.cpp
    src/keyboard.cpp
    src/screencapturer.cpp
    src/camera.cpp
    src/fileexporter.cpp
    src/depthcapturer.cpp
    external/lodepng/lodepng.cpp
)

target_include_directories(aerosynth_gtav PRIVATE 
    src
    external/
)

target_link_directories(aerosynth_gtav PRIVATE 
    external/scripthookv_sdk/lib
)

target_link_libraries(aerosynth_gtav PRIVATE
    ScriptHookV
    d3d11
    dxgi
)

set_target_properties(aerosynth_gtav PROPERTIES
    OUTPUT_NAME "aerosynth_gtav"
    SUFFIX ".asi"
)

install (TARGETS aerosynth_gtav DESTINATION /)
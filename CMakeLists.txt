cmake_minimum_required(VERSION 4.3.2)

project(ApexEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(apex 
    main.cpp
    Window.cpp
)

target_compile_options(apex PRIVATE
    -fno-exceptions
    -fno-rtti
)
if(WIN32)
    target_compile_definitions(apex PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()
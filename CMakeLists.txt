cmake_minimum_required(VERSION 4.3.2)

project(ApexEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Locate the Vulkan SDK. Honors the VULKAN_SDK environment variable that
# the LunarG installer sets. Provides the Vulkan::Vulkan imported target,
# which propagates both the include path and the loader library.
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan: ${Vulkan_VERSION} at ${Vulkan_INCLUDE_DIRS}")

add_executable(apex
    src/main.cpp
    src/Window.cpp
    src/Input.cpp
    src/Log.cpp
    src/Assert.cpp
    src/Renderer.cpp
)

target_compile_options(apex PRIVATE
    -fno-exceptions
    -fno-rtti
)

target_link_libraries(apex PRIVATE
    Vulkan::Vulkan
)

# ---- Shader Compilation ----
set(APEX_SHADER_SOURCES
    shaders/triangle.vert
    shaders/triangle.frag
)

set(APEX_SHADER_OUTPUTS "")
foreach(SHADER ${APEX_SHADER_SOURCES})
    set(SPV_OUTPUT "${CMAKE_BINARY_DIR}/${SHADER}.spv")
    add_custom_command(
        OUTPUT ${SPV_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -o ${SPV_OUTPUT} "${CMAKE_SOURCE_DIR}/${SHADER}"
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER}"
        VERBATIM
    )
    list(APPEND APEX_SHADER_OUTPUTS ${SPV_OUTPUT})
endforeach()

add_custom_target(apex_shaders ALL DEPENDS ${APEX_SHADER_OUTPUTS})
add_dependencies(apex apex_shaders)

if(WIN32)
    target_compile_definitions(apex PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()
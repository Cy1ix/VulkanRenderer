cmake_minimum_required(VERSION 3.20)
project(VulkanDemo)

add_compile_options(/utf-8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB MAIN_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/include/imgui/*.cpp"
)

add_executable(${PROJECT_NAME} ${MAIN_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include
    ${Vulkan_INCLUDE_DIRS}
)

file(GLOB COMMON_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/*.lib")
file(GLOB DEBUG_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/debug/*.lib")
file(GLOB RELEASE_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/release/*.lib")

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${COMMON_LIBS}
    
    $<$<CONFIG:Debug>:${DEBUG_LIBS}>
    $<$<NOT:$<CONFIG:Debug>>:${RELEASE_LIBS}>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    GLFW_INCLUDE_NONE
    GLFW_STATIC
    WIN32_LEAN_AND_MEAN
    VK_USE_PLATFORM_WIN32_KHR
    NOMINMAX
    GLM_ENABLE_EXPERIMENTAL
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
    MODEL_DIR=\"${CMAKE_SOURCE_DIR}/assets/models/\"
    SHADER_DIR=\"${CMAKE_SOURCE_DIR}/assets/shaders/\"
    TEXTURE_DIR=\"${CMAKE_SOURCE_DIR}/assets/texture/\"
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

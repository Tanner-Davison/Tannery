# Version->Project->Standard block -> find_package calls -> targets -> link/include wiring

# STANDARD BLOCK
# -----------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_COMPILER clang++)
project(tannery LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Makes it Fail-Loudly instead of silently downgrading
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Always generate compile_commands.json for clangd
# FIND PACKAGES
# -----------------------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(SOURCES
    core/src/main.cpp
    core/src/App.cpp
    core/src/VulkanInstance.cpp
    core/src/Window.cpp
    core/src/physicalDevice.cpp
    core/src/queueFamilies.cpp
    core/src/LogicalDevice.cpp
    core/src/Surface.cpp
    core/src/swapchainSupport.cpp
    core/src/Swapchain.cpp)
# TARGETS
# -------------------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCES})

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE) # Turns off deprecation warning

add_subdirectory(external/libs/minlog EXCLUDE_FROM_ALL)
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)

set(TF_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/libs/taskflow EXCLUDE_FROM_ALL)

# LINKING/INCLUDES
# -------------------------------------------------------------------------------------------

target_link_libraries(tannery PRIVATE glfw Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PUBLIC minlog)
target_link_libraries(${PROJECT_NAME} PUBLIC Taskflow)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC "-framework Metal" "-framework MetalKit"
                                                 "-framework Cocoa")
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_ENABLE_BETA_EXTENSIONS)
endif()

target_include_directories(
    ${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/core/src)

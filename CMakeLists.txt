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
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

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
set(SHADER_SOURCES data/shaders/triangle.vert data/shaders/triangle.frag)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders") # Creates shaders folder in build

set(SHADER_BINARIES "")

foreach(SHADER ${SHADER_SOURCES})
    # Extract just the filename (e.g., "triangle.vert") from the full path
    get_filename_component(SHADER_NAME "${SHADER}" NAME)

    # Compute the output path: ${CMAKE_BINARY_DIR}/shaders/triangle.vert
    set(OUTPUT_SPV "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

    # Use the absolute path of the source shader for dependencies
    get_filename_component(SHADER_PATH "${SHADER}" ABSOLUTE)

    add_custom_command(
        OUTPUT "${OUTPUT_SPV}"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_PATH}" -o "${OUTPUT_SPV}"
        DEPENDS "${SHADER_PATH}"
        COMMENT "Compiling shader ${SHADER_NAME}")
    # Append that .spv path to the running list
    list(APPEND SHADER_BINARIES "${OUTPUT_SPV}")
endforeach()

# TARGETS
# -------------------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCES})
add_custom_target(Shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(${PROJECT_NAME} Shaders)

# MINLOG
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE) # Turns off deprecation warning
add_subdirectory(deps/libs/minlog EXCLUDE_FROM_ALL) # minlog
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
#
# TASKFLOW
set(TF_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/libs/taskflow EXCLUDE_FROM_ALL) # taskflow

#
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

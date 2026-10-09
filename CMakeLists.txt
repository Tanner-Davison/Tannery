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
    core/src/Swapchain.cpp
    core/src/shaderModule.cpp
    core/src/Pipeline.cpp
    core/src/CommandBuffers.cpp
    core/src/SyncObjects.cpp
    core/src/vma.cpp
    core/src/Allocator.cpp
    core/src/Buffer.cpp
    core/src/GraphicsContext.cpp
    core/src/Mesh.cpp
    core/src/Renderer.cpp
    core/src/FrameDescriptors.cpp
    core/src/DepthImage.cpp
    core/src/stb_image.cpp
    core/src/Texture.cpp
    core/src/Sampler.cpp
    core/src/MaterialDescriptors.cpp
    core/src/Camera.cpp
    core/src/CameraController.cpp
    core/src/InputMap.cpp
    core/src/Settings.cpp
    core/src/GpuProfiler.cpp)

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

# TRACY (profiler, BSD-3-Clause). Client library only; the viewer is a separate program.
# Version must match the viewer you run: see documentation/profiling.md
option(FORGE3D_PROFILING "Instrument the engine with Tracy zones" ON)
set(TRACY_ENABLE ${FORGE3D_PROFILING} CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)       # collect nothing until the viewer connects
set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)  # never listen on the network
add_subdirectory(deps/tracy EXCLUDE_FROM_ALL)

# TASKFLOW
set(TF_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/taskflow EXCLUDE_FROM_ALL) # taskflow

#
# LINKING/INCLUDES
# -------------------------------------------------------------------------------------------

target_link_libraries(tannery PRIVATE glfw Vulkan::Vulkan Tracy::TracyClient)
target_link_libraries(${PROJECT_NAME} PUBLIC Taskflow)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC "-framework Metal" "-framework MetalKit"
                                                 "-framework Cocoa")
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_ENABLE_BETA_EXTENSIONS)

endif()

# Adds SHADER_DIR (compiled .spv) and TEXTURE_DIR (source images, read in place) to CMAKE
target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
            TEXTURE_DIR="${CMAKE_SOURCE_DIR}/data/textures" GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(
    ${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/core/src)

# stb is header-only; deps/ lets `#include <stb/stb_image.h>` resolve on every platform
target_include_directories(${PROJECT_NAME} SYSTEM
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# TESTS
# -------------------------------------------------------------------------------------------
# Engine tests cover only code with no GLFW/Vulkan dependency, so they need no window or GPU.
#   cmake --build build --target engine_tests && ctest --test-dir build -R engine_tests
# (or just run build/engine_tests; exit code 0 = all passed)
option(FORGE3D_BUILD_TESTS "Build the engine unit tests" ON)
if(FORGE3D_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests EXCLUDE_FROM_ALL
        tests/engine_tests.cpp core/src/Camera.cpp core/src/Settings.cpp)
    target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

# Specify CMake version(s)
cmake_minimum_required(VERSION 4.0)


# Declare project
project(
    FirstVulkanProject
    VERSION 1.0
    LANGUAGES CXX
)

# Compiler settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Default build type (adds -O3)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Declare the executable
add_executable(${PROJECT_NAME})

# Add source files
target_sources(${PROJECT_NAME} PRIVATE
    # ./src/
    src/main.cpp

)

# Add include subdirectories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

# Link external libraries

# Vulkan
find_package(Vulkan REQUIRED)

target_link_libraries (${PROJECT_NAME} Vulkan::Vulkan)

# GLFW
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.9
)

FetchContent_GetProperties(glfw)
if(NOT glfw_POPULATED)
    FetchContent_MakeAvailable(glfw)
endif()

target_link_libraries(${PROJECT_NAME} glfw)

# GLM
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #refs/tags/1.0.1
)

if(NOT glm_POPULATED)
    FetchContent_MakeAvailable(glm)
endif()

target_link_libraries(${PROJECT_NAME} glm::glm)

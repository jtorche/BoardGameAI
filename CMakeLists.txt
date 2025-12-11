cmake_minimum_required(VERSION 4.0.0)

project(BoardGameAI DESCRIPTION "BoardGame Artificial Intelligence project" LANGUAGES CXX)

# Only do these if this is the main project, and not if it is included through
# add_subdirectory
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # C++ Standard
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Ensure -std=c++xx instead of -std=g++xx
    set(CMAKE_CXX_EXTENSIONS OFF)

    # Nicely support folders in IDE's
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

option(USE_TORCH "Check it to enable Torch ML backend." ON)

if(USE_TORCH)
	find_package(Torch REQUIRED)
endif()

set(TINY_DNN_INCLUDE_DIR "" CACHE PATH "Path to Tiny DNN includes")

# Libraries code
add_subdirectory(src)

# Executables code
add_subdirectory(apps)
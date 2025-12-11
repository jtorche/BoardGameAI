project(Core DESCRIPTION "Core library" LANGUAGES CXX)

file(GLOB_RECURSE core_sources *.cpp *.h)

add_library(${PROJECT_NAME} ${core_sources})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Enabled warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)

# For nicer IDE views
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "libs")

source_group(
  TREE ${CMAKE_SOURCE_DIR}/src/Core
  PREFIX "Sources"
  FILES ${core_sources})
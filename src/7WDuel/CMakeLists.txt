project(7WDuelEngine DESCRIPTION "7WDuel Engine library" LANGUAGES CXX)

file(GLOB_RECURSE 7wduel_sources *.cpp *.h)

add_library(${PROJECT_NAME} ${7wduel_sources})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Enabled warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)

target_link_libraries(${PROJECT_NAME} PRIVATE Core)

# For nicer IDE views
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "libs")

source_group(
  TREE ${CMAKE_SOURCE_DIR}/src/7WDuel
  PREFIX "Sources"
  FILES ${7wduel_sources})
project(GuessNumberGame DESCRIPTION "GuessTheNumber Engine library" LANGUAGES CXX)

file(GLOB_RECURSE guessbn_sources *.cpp *.h)

add_library(${PROJECT_NAME} ${guessbn_sources})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Enabled warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)

target_link_libraries(${PROJECT_NAME} PRIVATE Core)

# For nicer IDE views
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "libs")

source_group(
  TREE ${CMAKE_SOURCE_DIR}/src/GuessNumber
  PREFIX "Sources"
  FILES ${guessbn_sources})
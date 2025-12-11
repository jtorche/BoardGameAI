project(AI DESCRIPTION "AI library" LANGUAGES CXX)

file(GLOB_RECURSE ai_sources *.cpp *.h)

add_library(${PROJECT_NAME} ${ai_sources})

target_precompile_headers(AI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/precompiled_header.h)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(${PROJECT_NAME} PUBLIC ${TINY_DNN_INCLUDE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
target_compile_options(AI PRIVATE /bigobj)

# Enabled warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)

target_link_libraries(${PROJECT_NAME} PRIVATE Core 7WDuelEngine)

# For nicer IDE views
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "libs")

source_group(
  TREE ${CMAKE_SOURCE_DIR}/src/AI
  PREFIX "Source Files"
  FILES ${ai_sources})
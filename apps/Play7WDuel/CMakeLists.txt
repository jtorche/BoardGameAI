project(Play7WDuel DESCRIPTION "Graphic application for 7WDuel" LANGUAGES CXX)

file(GLOB_RECURSE apps_sources *.cpp *.h)

add_executable(${PROJECT_NAME} ${apps_sources})

target_link_libraries(Play7WDuel PRIVATE SDL3::SDL3 ${SDL3_IMAGE_LIB} ${SDL3_TTF_LIB})

target_precompile_headers(Play7WDuel REUSE_FROM AI)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src ${SDL3_IMAGE_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
target_compile_options(Play7WDuel PRIVATE /bigobj)

# Enabled warnings
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>)

# Linked libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Core 7WDuelEngine AI)

# For nicer IDE views
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "apps")

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  PREFIX "Sources"
  FILES ${apps_sources})
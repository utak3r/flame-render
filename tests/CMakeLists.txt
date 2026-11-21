cmake_minimum_required(VERSION 3.24)

if(NOT TARGET GTest::gtest_main)
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(flame_render_tests
    test_flame_fractal.cpp
    test_renderer.cpp
    test_golden_samples.cpp
)

target_link_libraries(flame_render_tests PRIVATE
    flame_render::flame_render
    GTest::gtest_main
)

target_compile_definitions(flame_render_tests PRIVATE
    FLAME_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)

if(MSVC)
    target_compile_options(flame_render_tests PRIVATE /W4 /MP)
else()
    target_compile_options(flame_render_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_custom_command(TARGET flame_render_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:flame_render_tests>>,copy_if_different,true>
            $<TARGET_RUNTIME_DLLS:flame_render_tests>
            $<TARGET_FILE_DIR:flame_render_tests>
        COMMAND_EXPAND_LISTS
    )
endif()

include(GoogleTest)
gtest_discover_tests(flame_render_tests)

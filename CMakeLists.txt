cmake_minimum_required(VERSION 3.24)
project(flame_render VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

# 1. pugixml for XML parsing/serialization
if(NOT TARGET pugixml::pugixml)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
    )
    FetchContent_MakeAvailable(pugixml)
endif()

# 2. lodepng for PNG encoding/decoding
if(NOT TARGET lodepng)
    FetchContent_Declare(
        lodepng
        GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
        GIT_TAG master
        SOURCE_SUBDIR non_existent_dir
    )
    FetchContent_MakeAvailable(lodepng)
    add_library(lodepng STATIC "${lodepng_SOURCE_DIR}/lodepng.cpp")
    target_include_directories(lodepng PUBLIC "${lodepng_SOURCE_DIR}")
endif()

# Core Flame Fractal & Renderer library
add_library(flame_render STATIC
    src/core/FlameGradient.cpp
    src/core/FlameVariation.cpp
    src/core/FlameTransform.cpp
    src/core/FlameFractal.cpp
    src/core/FlameXmlSerializer.cpp
    src/renderer/FlameImage.cpp
    src/renderer/FlameToneMapper.cpp
    src/renderer/FlameCpuRenderer.cpp
)
add_library(flame_render::flame_render ALIAS flame_render)

target_compile_features(flame_render PUBLIC cxx_std_20)
set_target_properties(flame_render PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(flame_render PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(flame_render PRIVATE
    pugixml::pugixml
    lodepng
)

if(MSVC)
    target_compile_options(flame_render PRIVATE /W4 /MP)
else()
    target_compile_options(flame_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(FLAME_RENDER_BUILD_TESTS "Build flame_render unit and golden tests" ${PROJECT_IS_TOP_LEVEL})
if(FLAME_RENDER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

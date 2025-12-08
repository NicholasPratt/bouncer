cmake_minimum_required(VERSION 3.10)
project(SDLGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find SDL2_image (try config, then pkg-config if available)
find_package(SDL2_image QUIET CONFIG)
if(NOT SDL2_image_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)
        if(SDL2_IMAGE_FOUND)
            add_library(SDL2_image::SDL2_image INTERFACE IMPORTED)
            target_include_directories(SDL2_image::SDL2_image INTERFACE ${SDL2_IMAGE_INCLUDE_DIRS})
            target_link_libraries(SDL2_image::SDL2_image INTERFACE ${SDL2_IMAGE_LINK_LIBRARIES})
            target_compile_options(SDL2_image::SDL2_image INTERFACE ${SDL2_IMAGE_CFLAGS_OTHER})
        else()
            message(FATAL_ERROR "Could not find SDL2_image. Install SDL2_image dev package or set SDL2_image_DIR/CMAKE_PREFIX_PATH.")
        endif()
    else()
        message(FATAL_ERROR "Pkg-config not found and SDL2_image config missing. Install pkg-config plus SDL2_image dev package, or set SDL2_image_DIR/CMAKE_PREFIX_PATH.")
    endif()
endif()

# Find SDL2_ttf
find_package(SDL2_ttf REQUIRED)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link SDL2 and SDL2_ttf and SDL2_image
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

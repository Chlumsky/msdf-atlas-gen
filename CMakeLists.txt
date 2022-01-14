
cmake_minimum_required(VERSION 3.15)

option(MSDF_ATLAS_GEN_BUILD_STANDALONE "Build the msdf-atlas-gen standalone executable" ON)
option(MSDF_ATLAS_GEN_MSDFGEN_EXTERNAL "Do not build the msdfgen submodule but find it as an external package" OFF)
if(NOT MSDF_ATLAS_GEN_MSDFGEN_EXTERNAL)
    set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "Build the msdfgen standalone executable")
    set(MSDFGEN_USE_OPENMP OFF CACHE INTERNAL "Build with OpenMP support for multithreaded code (disabled for atlas gen)" FORCE)
    set(MSDFGEN_USE_CPP11 ON CACHE INTERNAL "Build with C++11 enabled (always enabled for atlas gen)" FORCE)
    set(MSDFGEN_INSTALL OFF CACHE BOOL "Generate installation target for msdfgen")
endif()

project(msdf-atlas-gen VERSION 1.2 LANGUAGES CXX)

find_package(Threads REQUIRED)

if(MSDF_ATLAS_GEN_MSDFGEN_EXTERNAL)
    find_package(msdfgen REQUIRED)
else()
    add_subdirectory(msdfgen)
endif()

file(GLOB_RECURSE MSDF_ATLAS_GEN_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "msdf-atlas-gen/*.h" "msdf-atlas-gen/*.hpp")
file(GLOB_RECURSE MSDF_ATLAS_GEN_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "msdf-atlas-gen/*.cpp")

# msdf-atlas-gen library
add_library(msdf-atlas-gen ${MSDF_ATLAS_GEN_HEADERS} ${MSDF_ATLAS_GEN_SOURCES})
add_library(msdf-atlas-gen::msdf-atlas-gen ALIAS msdf-atlas-gen)
set_target_properties(msdf-atlas-gen PROPERTIES PUBLIC_HEADER "${MSDF_ATLAS_GEN_HEADERS}")
target_include_directories(msdf-atlas-gen
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/include # for lodepng.h
        ${CMAKE_CURRENT_SOURCE_DIR}/artery-font-format
)

target_compile_features(msdf-atlas-gen PUBLIC cxx_std_11)
target_link_libraries(msdf-atlas-gen PUBLIC Threads::Threads msdfgen::msdfgen)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT msdf-atlas-gen)

# msdf-atlas-gen standalone executable
if(MSDF_ATLAS_GEN_BUILD_STANDALONE)
    set(MSDF_ATLAS_GEN_STANDALONE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen/main.cpp")
    if(MSVC)
        set(MSDF_ATLAS_GEN_STANDALONE_SOURCES ${MSDF_ATLAS_GEN_STANDALONE_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/msdf-atlas-gen.rc")
    endif()
    add_executable(msdf-atlas-gen-standalone ${MSDF_ATLAS_GEN_STANDALONE_SOURCES})
    target_compile_definitions(msdf-atlas-gen-standalone PUBLIC MSDF_ATLAS_STANDALONE)
    set_target_properties(msdf-atlas-gen-standalone PROPERTIES
        OUTPUT_NAME msdf-atlas-gen
        ARCHIVE_OUTPUT_NAME msdf-atlas-gen-standalone
        # Avoid deleting msdf-atlas-gen.lib during clean
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(msdf-atlas-gen-standalone PUBLIC msdf-atlas-gen::msdf-atlas-gen)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT msdf-atlas-gen-standalone)
endif()

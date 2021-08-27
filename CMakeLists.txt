cmake_minimum_required(VERSION 3.10)

project(msdf-atlas-gen LANGUAGES CXX)

add_subdirectory(msdfgen)
find_package(Threads REQUIRED)

#----------------------------------------------------------------
# Gathering files
#----------------------------------------------------------------

file(GLOB_RECURSE msdf-atlas-gen_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	"msdf-atlas-gen/*.cpp"
)

#----------------------------------------------------------------
# Target configuration
#----------------------------------------------------------------

add_executable(msdf-atlas-gen ${msdf-atlas-gen_SOURCES})

target_compile_definitions(msdf-atlas-gen
    PRIVATE
        MSDF_ATLAS_STANDALONE
)

target_link_libraries(msdf-atlas-gen
    PRIVATE
        Threads::Threads
        msdfgen::msdfgen
        msdfgen::msdfgen-ext
)

target_include_directories(msdf-atlas-gen
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/artery-font-format"
        "${CMAKE_CURRENT_SOURCE_DIR}/msdfgen/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/msdfgen"
)

#----------------------------------------------------------------
# Installation and exportation of the executable
#----------------------------------------------------------------

install(TARGETS msdf-atlas-gen)

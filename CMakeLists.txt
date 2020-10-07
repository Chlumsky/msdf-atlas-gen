cmake_minimum_required(VERSION 3.10)

project(msdf-atlas-gen)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if (COMPILER_SUPPORTS_CXX11)
	add_definitions(-DMSDFGEN_USE_CPP11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# Make release mode default (turn on optimizations)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package (Threads REQUIRED)

#----------------------------------------------------------------
# Support Functions
#----------------------------------------------------------------

# Mirror the folder structure for sources inside the IDE...
function(folderize_sources sources prefix)
	foreach(FILE ${${sources}}) 
	  get_filename_component(PARENT_DIR "${FILE}" PATH)

	  # skip src or include and changes /'s to \\'s
	  string(REPLACE "${prefix}" "" GROUP "${PARENT_DIR}")
	  string(REPLACE "/" "\\" GROUP "${GROUP}")

      # If it's got a path, then append a "\\" separator (otherwise leave it blank)
	  if ("${GROUP}" MATCHES ".+")
	  	set(GROUP "\\${GROUP}")
	  endif()

	  source_group("${GROUP}" FILES "${FILE}")
	endforeach()
endfunction(folderize_sources)



file(GLOB_RECURSE msdfgen_HEADERS
	msdf-atlas-gen/*.h
	msdf-atlas-gen/*.hpp
)

file(GLOB_RECURSE msdfgen_SOURCES
	msdf-atlas-gen/*.cpp
)

include_directories (
	artery-font-format
    msdfgen
    msdfgen/include
)

add_subdirectory (msdfgen)

# Build the library (aliased name because it's the same target name the exe)
folderize_sources(msdf-atlas-gen_HEADERS ${CMAKE_SOURCE_DIR})
folderize_sources(msdf-atlas-gen_SOURCES ${CMAKE_SOURCE_DIR})

add_library(lib_msdf-atlas-gen ${msdfgen_SOURCES} ${msdfgen_HEADERS})
set_target_properties(lib_msdf-atlas-gen PROPERTIES OUTPUT_NAME msdf-atlas-gen)
target_link_libraries(lib_msdf-atlas-gen msdfgen::msdfgen msdfgen::msdfgen-ext)

# Build the executable

add_executable(msdf-atlas-gen msdf-atlas-gen/main.cpp)
target_compile_definitions(msdf-atlas-gen PRIVATE MSDF_ATLAS_STANDALONE)
target_link_libraries(msdf-atlas-gen lib_msdf-atlas-gen msdfgen::msdfgen msdfgen::msdfgen-ext ${CMAKE_THREAD_LIBS_INIT})

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -std=c++17 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -std=c++17")
set(CMAKE_CXX_STANDARD 17)

set(EMSCRIPTEN_ROOT $ENV{EMSDK}/upstream/emscripten CACHE STRING "Emscripten path")
set(CMAKE_TOOLCHAIN_FILE ${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake)

set(SPRINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../spine-cpp/spine-cpp")

project("spine-wasm")


file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB_RECURSE SPINE_FILES
    ${SPRINE_DIR}/include/*/*.h
    ${SPRINE_DIR}/src/*/*.cpp
)


include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SPRINE_DIR}/Include/
)

SET(EXE_NAME "Spine")

#ADD_LIBRARY(${EXE_NAME} ${SRC_FILES} ${SPINE_FILES})


# Build Release by default
# set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type")


set(EMCC_GLUE_ARGS
  -c
  -I${RECAST_SRC_DIR}
  -I${SPRINE_DIR}/Include/
)

#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EMCC_WASM_ESM_ARGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMCC_WASM_ESM_ARGS}")

add_executable(${EXE_NAME} ${SRC_FILES} ${SPINE_FILES})


if(${CMAKE_BUILD_TYPE} STREQUAL "release")
message("CMAKE_BUILD_TYPE: Release")
  set_target_properties(${EXE_NAME} PROPERTIES
    LINK_FLAGS "--bind  -s EXPORT_NAME=Spine -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS=_malloc,_free -s MODULARIZE=1 -O0"
  )
else()
message("CMAKE_BUILD_TYPE: Debug")
  set_target_properties(${EXE_NAME} PROPERTIES
    LINK_FLAGS "--bind  -s EXPORT_NAME=Spine -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS=_malloc,_free -s MODULARIZE=1 -O3"
  )
endif()
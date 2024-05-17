cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -std=c++17 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -std=c++17")
set(CMAKE_CXX_STANDARD 17)

set(EMSCRIPTEN_ROOT $ENV{EMSDK}/upstream/emscripten CACHE STRING "Emscripten path")
set(CMAKE_TOOLCHAIN_FILE ${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake)

set(SPRINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../spine-cpp/spine-cpp")

project("spine-wasm")

message("${PROGAME_NAME}")

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

SET(EXE_NAME Spine)


set(EMCC_GLUE_ARGS
  -c
  -I${RECAST_SRC_DIR}
  -I${SPRINE_DIR}/Include/
)

add_executable(${EXE_NAME} ${SRC_FILES} ${SPINE_FILES})

set(FLAG "--bind -s EXPORT_NAME=Spine -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS=_malloc,_free -s MODULARIZE=1 ")

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
if(${CMAKE_BUILD_TYPE} STREQUAL "RELEASE")
  string(APPEND FLAG " -O3")
  string(APPEND FLAG " -g0")
  string(APPEND FLAG " -sMODULARIZE")
else()
  string(APPEND FLAG " -g3")
  string(APPEND FLAG " -O0")
endif()

if(${VERSION} STREQUAL "js")
  string(APPEND FLAG " -s WASM=0")
endif()

message("FLAG: ${FLAG}")
set_target_properties(${EXE_NAME} PROPERTIES
    LINK_FLAGS ${FLAG}
)

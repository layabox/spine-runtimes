#!/bin/sh
clear
mkdir -p ./build
mkdir -p dist
cmake ./spine-laya   -B ./build/release   -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=RELEASE  -DTARGET_BUILD_PLATFORM=emscripten -DCMAKE_TOOLCHAIN_FILE=~/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=./dist  -DCMAKE_BUILD_TYPE=release -DCMAKE_CROSSCOMPILING_EMULATOR=/home/ubuntu/emsdk/node/16.20.0_64bit/bin/node
cd ./build/release
emmake make -j4
cd ../../dist
cp ../build/release/spine.js .
cp ../build/release/spine.wasm .
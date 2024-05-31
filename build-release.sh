#!/bin/sh
clear
mkdir -p ./build
mkdir -p dist
cmake ./spine-laya  -B ./build/release  -G "MinGW Makefiles" -DVERSION=wasm -DTARGET_BUILD_PLATFORM=emscripten -DCMAKE_TOOLCHAIN_FILE=~/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=./dist  -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_CROSSCOMPILING_EMULATOR=/home/ubuntu/emsdk/node/16.20.0_64bit/bin/node
cd ./build/release
emmake make -j4
cd ../../dist
cp ../build/release/Spine.js .
cp ../build/release/Spine.wasm .


sed -i "s/Spine.wasm/spine.wasm_3.8.wasm/g" Spine.js

mv Spine.wasm spine.wasm_3.8.wasm
mv Spine.js spine.wasm_3.8.js 

cd ../

cmake ./spine-laya  -B ./build/release  -G "MinGW Makefiles" -DVERSION=js -DTARGET_BUILD_PLATFORM=emscripten -DCMAKE_TOOLCHAIN_FILE=~/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=./dist  -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_CROSSCOMPILING_EMULATOR=/home/ubuntu/emsdk/node/16.20.0_64bit/bin/node
cd ./build/release
emmake make -j4
cd ../../dist
cp ../build/release/Spine.js .
cp ../build/release/Spine.js.mem .

sed -i "s/Spine.js.mem/spine_3.8.js.mem/g" Spine.js

mv Spine.js spine_3.8.js 
mv Spine.js.mem spine_3.8.js.mem

cmake_minimum_required(VERSION 3.15)
project(xmrig-wasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be built with Emscripten. Use emcmake cmake")
endif()

set(SOURCES
    src/xmrig_wasm_miner.cpp
    src/stratum_client.cpp
)

add_executable(xmrig-wasm ${SOURCES})

target_include_directories(xmrig-wasm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(xmrig-wasm PRIVATE
    -O3
    -flto
    -pthread
    -matomics
    -mbulk-memory
    -Wall
)

target_compile_definitions(xmrig-wasm PRIVATE
    EMSCRIPTEN
    XMRIG_WASM
)

set(WASM_LINK_FLAGS
    "-O3"
    "-flto"
    "-s WASM=1"
    "-s EXPORTED_FUNCTIONS=['_wasmStartMining','_wasmStopMining','_wasmGetHashrate','_wasmGetTotalHashes','_wasmGetSharesFound','_wasmIsRunning','_wasmGetConnectionStatus','_malloc','_free']"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8']"
    "-s USE_PTHREADS=1"
    "-s PTHREAD_POOL_SIZE=4"
    "-s PTHREAD_POOL_DELAY_LOAD=1"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=128MB"
    "-s MAXIMUM_MEMORY=1GB"
    "-s STACK_SIZE=8MB"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME=XMRigModule"
    "-s ENVIRONMENT=web,worker"
    "-s NO_FILESYSTEM=1"
    "-lwebsocket.js"
    "--bind"
    "--no-entry"
)

string(REPLACE ";" " " WASM_LINK_FLAGS_STR "${WASM_LINK_FLAGS}")
set_target_properties(xmrig-wasm PROPERTIES LINK_FLAGS "${WASM_LINK_FLAGS_STR}")

set_target_properties(xmrig-wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

message(STATUS "XMRig-WASM configured")

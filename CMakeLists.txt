cmake_minimum_required(VERSION 3.15)
project(xmrig-wasm-minimal)

set(CMAKE_CXX_STANDARD 17)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use emcmake cmake")
endif()

add_executable(xmrig-wasm src/minimal_randomx_miner.cpp)

target_compile_options(xmrig-wasm PRIVATE
    -O3
    -pthread
    -matomics
)

set(LINK_FLAGS
    "-O3"
    "-s WASM=1"
    "-s EXPORTED_FUNCTIONS=['_wasmStartMining','_wasmStopMining','_wasmGetHashrate','_wasmGetTotalHashes','_wasmGetSharesFound','_malloc','_free']"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-s USE_PTHREADS=1"
    "-s PTHREAD_POOL_SIZE=4"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=64MB"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME=XMRigModule"
    "-s ENVIRONMENT=web,worker"
    "--bind"
)

string(JOIN " " LINK_FLAGS_STR ${LINK_FLAGS})
set_target_properties(xmrig-wasm PROPERTIES LINK_FLAGS ${LINK_FLAGS_STR})

set_target_properties(xmrig-wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

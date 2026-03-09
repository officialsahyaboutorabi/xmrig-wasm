cmake_minimum_required(VERSION 3.15)
project(xmrig-wasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be built with Emscripten. Use emcmake cmake")
endif()

# RandomX source files for interpreted light mode (no JIT, no assembly)
set(RANDOMX_SOURCES
    # Core RandomX implementation
    src/xmrig/src/3rdparty/randomx/src/randomx.cpp
    src/xmrig/src/3rdparty/randomx/src/virtual_machine.cpp
    src/xmrig/src/3rdparty/randomx/src/vm_interpreted.cpp
    src/xmrig/src/3rdparty/randomx/src/vm_interpreted_light.cpp
    src/xmrig/src/3rdparty/randomx/src/bytecode_machine.cpp
    src/xmrig/src/3rdparty/randomx/src/instructions_portable.cpp
    
    # AES (software only - no hardware AES in WASM)
    src/xmrig/src/3rdparty/randomx/src/soft_aes.cpp
    src/xmrig/src/3rdparty/randomx/src/aes_hash.cpp
    
    # Dataset and cache
    src/xmrig/src/3rdparty/randomx/src/dataset.cpp
    src/xmrig/src/3rdparty/randomx/src/superscalar.cpp
    src/xmrig/src/3rdparty/randomx/src/blake2_generator.cpp
    
    # Argon2 for cache initialization
    src/xmrig/src/3rdparty/randomx/src/argon2_ref.c
    src/xmrig/src/3rdparty/randomx/src/argon2_core.c
    
    # Blake2b hashing
    src/xmrig/src/3rdparty/randomx/src/blake2/blake2b.c
    
    # Other utilities
    src/xmrig/src/3rdparty/randomx/src/reciprocal.c
    src/xmrig/src/3rdparty/randomx/src/instruction.cpp
    src/xmrig/src/3rdparty/randomx/src/virtual_memory.c
    src/xmrig/src/3rdparty/randomx/src/allocator.cpp
    src/xmrig/src/3rdparty/randomx/src/cpu.cpp
)

# Main application sources
set(APP_SOURCES
    src/xmrig_wasm_miner.cpp
    src/stratum_client.cpp
    src/randomx_wasm.cpp
)

add_executable(xmrig-wasm ${APP_SOURCES} ${RANDOMX_SOURCES})

target_include_directories(xmrig-wasm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xmrig/src/3rdparty/randomx/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xmrig/src/3rdparty/randomx/src/blake2
)

target_compile_options(xmrig-wasm PRIVATE
    -O2
    -pthread
    -matomics
    -mbulk-memory
    -Wall
    -Wno-unused-function
    -Wno-unused-variable
)

target_compile_definitions(xmrig-wasm PRIVATE
    EMSCRIPTEN
    XMRIG_WASM
    RANDOMX_NO_JIT=1
)

set(WASM_LINK_FLAGS
    "-O2"
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

# Copy web interface files to dist
add_custom_command(TARGET xmrig-wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/xmrig-web-miner.js
        ${CMAKE_CURRENT_SOURCE_DIR}/dist/xmrig-web-miner.js
    COMMENT "Copying web miner JS"
)

message(STATUS "XMRig-WASM configured with RandomX (light mode)")

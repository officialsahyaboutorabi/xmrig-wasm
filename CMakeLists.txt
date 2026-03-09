# XMRig WebAssembly Build
# This CMake file configures XMRig for Emscripten/WebAssembly compilation

cmake_minimum_required(VERSION 3.15)
project(xmrig-wasm VERSION 6.21.0 LANGUAGES C CXX)

# Emscripten specific settings
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

# Check if using Emscripten
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be built with Emscripten. Run: emcmake cmake")
endif()

# Include XMRig's source
set(XMRIG_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/xmrig/src)

# Define sources (simplified for WASM - no assembly)
file(GLOB XMRIG_SOURCES
    ${XMRIG_SRC_DIR}/core/*.cpp
    ${XMRIG_SRC_DIR}/crypto/*.cpp
    ${XMRIG_SRC_DIR}/crypto/rx/*.cpp
    ${XMRIG_SRC_DIR}/net/*.cpp
    ${XMRIG_SRC_DIR}/base/io/*.cpp
    ${XMRIG_SRC_DIR}/base/kernel/*.cpp
    ${XMRIG_SRC_DIR}/base/net/*.cpp
    ${XMRIG_SRC_DIR}/base/tools/*.cpp
)

# Exclude platform-specific files
list(REMOVE_ITEM XMRIG_SOURCES
    ${XMRIG_SRC_DIR}/crypto/common/VirtualMemory_unix.cpp
    ${XMRIG_SRC_DIR}/crypto/common/VirtualMemory_win.cpp
    ${XMRIG_SRC_DIR}/crypto/common/Assembly.cpp
)

# RandomX sources
set(RANDOMX_SRC_DIR ${XMRIG_SRC_DIR}/3rdparty/randomx/src)
file(GLOB RANDOMX_SOURCES
    ${RANDOMX_SRC_DIR}/*.c
    ${RANDOMX_SRC_DIR}/*.cpp
)

# Exclude assembly files and platform-specific
list(REMOVE_ITEM RANDOMX_SOURCES
    ${RANDOMX_SRC_DIR}/jit_compiler_x86.cpp
    ${RANDOMX_SRC_DIR}/jit_compiler_a64.cpp
    ${RANDOMX_SRC_DIR}/virtual_memory.cpp
    ${RANDOMX_SRC_DIR}/dataset.cpp  # Will use simplified version
)

# Create WASM-compatible VirtualMemory implementation
set(WASM_COMPAT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_compat/VirtualMemory_wasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_compat/dataset_wasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_compat/jit_compiler_wasm.cpp
)

# Main executable
add_executable(xmrig-wasm
    ${XMRIG_SOURCES}
    ${RANDOMX_SOURCES}
    ${WASM_COMPAT_SOURCES}
    src/wasm_main.cpp
)

# Include directories
target_include_directories(xmrig-wasm PRIVATE
    ${XMRIG_SRC_DIR}
    ${XMRIG_SRC_DIR}/3rdparty/randomx/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
target_compile_definitions(xmrig-wasm PRIVATE
    XMRIG_WASM
    XMRIG_NO_ASM
    RANDOMX_NO_JIT
    HAVE_DECL_STRNLEN=1
    _GNU_SOURCE
)

# Compiler flags for WASM
target_compile_options(xmrig-wasm PRIVATE
    -O3
    -flto
    -s USE_PTHREADS=1
    -pthread
    -matomics
    -mbulk-memory
    -msimd128
    -msse
    -msse2
    -msse3
    -mssse3
    -msse4.1
    -msse4.2
    -mavx
    -maes
    -fno-exceptions
    -fno-rtti
    -Wno-unused-command-line-argument
)

# Linker flags for WASM
target_link_options(xmrig-wasm PRIVATE
    -O3
    -flto
    -s WASM=1
    -s EXPORTED_FUNCTIONS='["_main", "_startMining", "_stopMining", "_getHashrate", "_getTotalHashes", "_setThreads", "_malloc", "_free"]'
    -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "UTF8ToString", "stringToUTF8", "getValue", "setValue", "lengthBytesUTF8"]'
    -s USE_PTHREADS=1
    -s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency
    -s PTHREAD_POOL_DELAY_LOAD=1
    -s ALLOW_MEMORY_GROWTH=1
    -s INITIAL_MEMORY=256MB
    -s MAXIMUM_MEMORY=2GB
    -s STACK_SIZE=8MB
    -s MODULARIZE=1
    -s EXPORT_NAME="XMRigModule"
    -s ENVIRONMENT=web,worker
    -s NO_FILESYSTEM=1
    -s NO_EXIT_RUNTIME=1
    -s ERROR_ON_UNDEFINED_SYMBOLS=0
    -s LLD_REPORT_UNDEFINED
    -s ASYNCIFY=1
    -s ASYNCIFY_IMPORTS=['emscripten_sleep']
    --bind
    --no-entry
)

# Link libraries
target_link_libraries(xmrig-wasm
    -lm
)

# Output to dist directory
set_target_properties(xmrig-wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

message(STATUS "XMRig-WASM configuration complete")
message(STATUS "Build with: emmake make -j$(nproc)")

cmake_minimum_required(VERSION 3.20)
project(BenchForge VERSION 0.1.0 LANGUAGES CXX)

# ─── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Output Directories ──────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ─── Third Party Dependencies ────────────────────────────────────────────────

# cpp-httplib (header-only)
# Make sure to: git submodule add https://github.com/yhirose/cpp-httplib third_party/cpp-httplib
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib)

# nlohmann/json (header-only)
# Make sure to: git submodule add https://github.com/nlohmann/json third_party/nlohmann
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party/nlohmann/include)

# toml++ (header-only)
# Make sure to: git submodule add https://github.com/marzer/tomlplusplus third_party/tomlplusplus
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${CMAKE_SOURCE_DIR}/third_party/tomlplusplus/include)

# SQLiteCpp
# Make sure to: git submodule add https://github.com/SRombauts/SQLiteCpp third_party/SQLiteCpp
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITE_ENABLE_COLUMN_METADATA OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/SQLiteCpp)

# ─── Source Files ────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    src/config/config.cpp
    src/db/database.cpp
    src/db/crud.cpp
    src/discovery/scanner.cpp
    src/benchmark/runner.cpp
    src/benchmark/metrics.cpp
    src/benchmark/perplexity.cpp
    src/export/exporter.cpp
    src/server/server.cpp
    src/server/routes.cpp
    src/utils/system_info.cpp
)

# ─── Executable ──────────────────────────────────────────────────────────────
add_executable(BenchForge ${SOURCES})

# ─── Include Directories ─────────────────────────────────────────────────────
target_include_directories(BenchForge PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann/include
    ${CMAKE_SOURCE_DIR}/third_party/tomlplusplus/include
)

# ─── Link Libraries ──────────────────────────────────────────────────────────
target_link_libraries(BenchForge PRIVATE
    httplib
    nlohmann_json
    tomlplusplus
    SQLiteCpp
)

# ─── Windows-Specific Settings ───────────────────────────────────────────────
if(WIN32)
    # Link against Winsock2 (required by cpp-httplib on Windows)
    target_link_libraries(BenchForge PRIVATE ws2_32)

    # Link against NVML for VRAM querying (optional, skip if not found)
    find_library(NVML_LIB nvml
        PATHS
            "C:/Program Files/NVIDIA Corporation/NVSMI"
            "C:/Windows/System32"
        NO_DEFAULT_PATH
    )
    if(NVML_LIB)
        message(STATUS "NVML found: ${NVML_LIB} -- GPU VRAM monitoring enabled")
        target_link_libraries(BenchForge PRIVATE ${NVML_LIB})
        target_compile_definitions(BenchForge PRIVATE BENCHFORGE_NVML_ENABLED)
    else()
        message(STATUS "NVML not found -- GPU VRAM monitoring disabled (CPU-only mode)")
    endif()

    # Suppress MSVC warnings for external headers
    if(MSVC)
        target_compile_options(BenchForge PRIVATE /W3 /WX- /wd4996)
        # Use static runtime to reduce DLL dependencies in release builds
        set_property(TARGET BenchForge PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET SQLiteCpp PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# ─── Copy Frontend to Build Directory ────────────────────────────────────────
# The backend serves frontend/ as static files at runtime
add_custom_command(TARGET BenchForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/frontend
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/frontend
    COMMENT "Copying frontend static files to build output..."
)

# ─── Copy config.toml to Build Directory ─────────────────────────────────────
add_custom_command(TARGET BenchForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.toml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.toml
    COMMENT "Copying config.toml to build output..."
)

# ─── Copy reference text for perplexity ──────────────────────────────────────
add_custom_command(TARGET BenchForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/reference
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/reference
    COMMENT "Copying reference texts to build output..."
)

# ─── Build Info ──────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== BenchForge v${PROJECT_VERSION} ===")
message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Output dir : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
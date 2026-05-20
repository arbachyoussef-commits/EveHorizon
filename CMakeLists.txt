cmake_minimum_required(VERSION 3.19)

project(ESTool LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force find_package to look for static libraries for GTest and GMock
set(GTest_USE_STATIC_LIBS ON)
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# On MSVC, static GTest requires the static C/C++ runtime (/MT or /MTd)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Helpful for IDE integration (VS Code / Clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure we are using a capable compiler for C++20 Coroutines
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
        message(FATAL_ERROR "GCC version must be at least 10.0 for C++20 coroutines!")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(MSVC_VERSION LESS 1928)
        message(FATAL_ERROR "MSVC version must be at least 16.8 (Visual Studio 2019) for C++20 coroutines!")
    endif()
endif()

# Compiler-specific flags for Coroutines and Warnings
if(MSVC)
    # /utf-8 ensures source files are handled correctly across locales
    add_compile_options(/W4 /Zc:__cplusplus /await:strict /utf-8)
else()
    add_compile_options(-Wall -Wextra -fcoroutines -fdiagnostics-color=always)
endif()

# # Force static linking of system libs (pthreads, libstdc++) on MinGW/GCC
# if(NOT MSVC AND WIN32)
#     add_link_options(-static)
# endif()

# Provide a hint for GTest if on Windows/MSYS2
if(WIN32)
    set(GTEST_ROOT "C:/msys64/mingw64" CACHE PATH "Path to GTest installation")
endif()

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE TEST_SOURCES 
    "Test/*.cpp"
)

enable_testing()
include(GoogleTest)

# Generate an executable for each test file
foreach(SOURCE_FILE ${TEST_SOURCES})
    get_filename_component(TARGET_NAME ${SOURCE_FILE} NAME_WE)
    
    # Since headers are included by the test cpp files directly, no app source list is needed
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/FiniteES
        ${CMAKE_CURRENT_SOURCE_DIR}/InfiniteES
        ${CMAKE_CURRENT_SOURCE_DIR}/Conversion
        ${GTEST_INCLUDE_DIRS}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE 
        GTest::gtest_main 
        GTest::gmock 
        Threads::Threads
    )

    if(UNIX AND NOT APPLE)
        target_link_options(${TARGET_NAME} PRIVATE -static-libgcc -static-libstdc++)
    endif()

    # CRITICAL DLL FIX FOR HEADER-ONLY PROJECT:
    # If find_package forces a dynamic GoogleTest dependency or MinGW runtime DLLs are needed,
    # this will safely copy them into the /build folder right next to your test executable.
    if(WIN32)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${TARGET_NAME}>
                $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMAND_EXPAND_LISTS
        )
    endif()

    # Postpones test discovery to runtime execution, bypassing configuration-time crashes [1].
    gtest_discover_tests(${TARGET_NAME} DISCOVERY_MODE PRE_TEST)
endforeach()
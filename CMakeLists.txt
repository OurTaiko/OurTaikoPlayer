cmake_minimum_required(VERSION 3.14)
project(OurTaiko VERSION 1.0.2 LANGUAGES C CXX)
set(PROJECT_AUTHOR "OurTaiko")

cmake_policy(SET CMP0002 NEW)  # Logical target names must be globally unique
cmake_policy(SET CMP0079 NEW)  # Allow target_link_libraries() everywhere

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "")

include(cmake/generate_icons.cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)
if(IOS)
  # Device and Simulator can both be arm64 but cannot share dependency binaries.
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Dependency build directory")
else()
  set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake-deps" CACHE PATH "" FORCE)
endif()

set(CMAKE_SKIP_INSTALL_RULES ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
  file(STRINGS "${CMAKE_SOURCE_DIR}/.env" _env_lines)
  foreach(_env_line ${_env_lines})
    if(_env_line MATCHES "^[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]*(.*)$")
      set(_env_key "${CMAKE_MATCH_1}")
      string(REGEX REPLACE "^[\"']|[\"']$" "" _env_value "${CMAKE_MATCH_2}")
      set(_env_${_env_key} "${_env_value}")
    endif()
  endforeach()
endif()

# Fanmade endpoints are configured at runtime in TOML.
option(FANMADE_NETWORK "Enable native OurTaiko Fanmade client" ON)
if(EMSCRIPTEN)
  set(FANMADE_NETWORK OFF)
endif()

set(DATATABLE_SEED "${_env_DATATABLE_SEED}" CACHE STRING "Decryption seed for the gen 4 datatable")
set(FUMEN_SEED "${_env_FUMEN_SEED}" CACHE STRING "Decryption seed for gen 4 fumen")
if(DATATABLE_SEED AND FUMEN_SEED)
  set(SUPPORT_FUMEN ON)
else()
  set(SUPPORT_FUMEN OFF)
endif()

if(IOS)
  # Xcode upgrades remove versioned SDK directories. Let find_package/find_library
  # rediscover those dependencies instead of retaining paths into the old SDK.
  get_cmake_property(_ios_cache_variables CACHE_VARIABLES)
  foreach(_ios_cache_variable IN LISTS _ios_cache_variables)
    get_property(_ios_cache_type CACHE "${_ios_cache_variable}" PROPERTY TYPE)
    if(_ios_cache_type MATCHES "^(PATH|FILEPATH)$"
        AND "${${_ios_cache_variable}}" MATCHES "/SDKs/[^/]+\\.sdk/"
        AND NOT EXISTS "${${_ios_cache_variable}}")
      message(STATUS "Clearing stale SDK cache entry: ${_ios_cache_variable}")
      unset(${_ios_cache_variable} CACHE)
    endif()
  endforeach()
  if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "16.3")
    message(FATAL_ERROR "OurTaiko requires iOS 16.3+ for C++20 floating-point formatting")
  endif()
  # Never resolve macOS Homebrew libraries while cross-compiling.
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  if(FANMADE_NETWORK)
    message(STATUS "iOS online services enabled (CPR/curl with Apple native TLS)")
  else()
    message(STATUS "iOS online services disabled: set FANMADE_NETWORK=ON to enable")
  endif()
endif()

include(cmake/deps.cmake)
include(cmake/codegen.cmake)

### Source files

file(GLOB_RECURSE CppFiles src/*.cpp)
file(GLOB_RECURSE HeaderFiles
    src/*.h
    src/*.hpp
)

list(FILTER CppFiles EXCLUDE REGEX "/src/libs/optional/")
if(SUPPORT_FUMEN)
  file(GLOB Gen4CppFiles src/libs/optional/*.cpp)
  list(APPEND CppFiles ${Gen4CppFiles})
endif()

message(STATUS "C++ files found: ${CppFiles}")

if(ANDROID)
  add_library(${PROJECT_NAME} SHARED ${CppFiles} ${HeaderFiles})
else()
  add_executable(${PROJECT_NAME} ${CppFiles} ${HeaderFiles})
endif()
add_dependencies(${PROJECT_NAME} skin_config_gen)

# Ship attribution with local desktop builds as well as CI release archives.
if(NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/NOTICE"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    VERBATIM)
endif()

if(WIN32)
  enable_language(RC)
  configure_file(windows/OurTaiko.rc.in "${CMAKE_BINARY_DIR}/OurTaiko.rc" @ONLY)
  target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/OurTaiko.rc")
endif()

if(MINGW)
  target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

if(WIN32)
  foreach(_lib avformat avcodec avutil swscale swresample)
    file(GLOB _dll "${FFMPEG_LIB_DIR}/../bin/${_lib}*.dll")
    if(_dll)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_dll}
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
    else()
      message(WARNING "FFmpeg DLL not found for ${_lib}")
    endif()
  endforeach()

  get_filename_component(_mingw_bin_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
  set(_runtime_dlls libstdc++-6.dll libwinpthread-1.dll libgcc_s_seh-1.dll libsamplerate-0.dll libiconv-2.dll)
  foreach(_rtdll ${_runtime_dlls})
    if(EXISTS "${_mingw_bin_dir}/${_rtdll}")
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_mingw_bin_dir}/${_rtdll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
    else()
      message(WARNING "Runtime DLL not found: ${_rtdll}")
    endif()
  endforeach()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${raylib_SOURCE_DIR}/src/external
    ${rapidjson_SOURCE_DIR}/include
    ${tomlplusplus_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    lua::lua
    sol2::sol2
    SQLite3_lib
    miniz
)


if(NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::swscale
    FFmpeg::avutil
    FFmpeg::swresample
)

target_link_libraries(${PROJECT_NAME} PRIVATE sndfile samplerate)
if(FANMADE_NETWORK)
  target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
  target_compile_definitions(${PROJECT_NAME} PRIVATE FANMADE_NETWORK)
endif()
if(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE iconv)
endif()

if(NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME} PRIVATE rtaudio)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE portaudio)

target_link_libraries(${PROJECT_NAME} PRIVATE g719)
target_compile_definitions(${PROJECT_NAME} PRIVATE YATAIDON_G719)

if(SUPPORT_FUMEN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    SUPPORT_FUMEN
    DATATABLE_SEED="${DATATABLE_SEED}"
    FUMEN_SEED="${FUMEN_SEED}"
  )
endif()

include(cmake/platform.cmake)
include(cmake/icons.cmake)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running ${PROJECT_NAME}..."
)

message(STATUS "")
message(STATUS "===============================================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32 AND NOT ANDROID AND NOT IOS)
  message(STATUS "Sanitizers: ${_debug_sanitizers}")
endif()
message(STATUS "===============================================================")
message(STATUS "")

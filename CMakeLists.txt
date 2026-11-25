cmake_minimum_required(VERSION 3.21)

project(node_datachannel VERSION 1.0.1)

# compile_commands.json
# -----------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# -Dnapi_build_version=8
add_definitions(-DNAPI_VERSION=8 -DNODE_ADDON_API_CPP_EXCEPTIONS_ALL)

include_directories(${CMAKE_JS_INC})

if(MSVC)
    set(OPENSSL_MSVC_STATIC_RT TRUE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=unused-but-set-variable -Wno-error=strict-prototypes")
endif()

# Add -frtti for non-MSVC toolchains (Linux, macOS, Android, MinGW)
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
endif()

if(NOT USE_GNUTLS AND NOT USE_MBEDTLS)
    option(OPENSSL_USE_STATIC_LIBS "Link OpenSSL statically" ON)
    find_package(OpenSSL REQUIRED)
endif()

include(FetchContent)

# Fetch libdatachannel
FetchContent_Declare(
    libdatachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG v0.24.8
    EXCLUDE_FROM_ALL
)

option(NO_MEDIA "Disable media transport support in libdatachannel" OFF)
option(NO_WEBSOCKET "Disable WebSocket support in libdatachannel" OFF)
set(NO_EXAMPLES ON CACHE BOOL "Disable libdatachannel examples" FORCE)
set(NO_TESTS ON CACHE BOOL "Disable libdatachannel tests" FORCE)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(libdatachannel)

# Create Source File List
set(SRC_FILES
    src/cpp/rtc-wrapper.cpp
    src/cpp/data-channel-wrapper.cpp
    src/cpp/ice-udp-mux-listener-wrapper.cpp
    src/cpp/peer-connection-wrapper.cpp
    src/cpp/thread-safe-callback.cpp
    src/cpp/main.cpp
    ${CMAKE_JS_SRC}
)

if(NOT NO_MEDIA)
    list(APPEND SRC_FILES
        src/cpp/media-direction.cpp
        src/cpp/media-handlers-wrapper.cpp
        src/cpp/media-mediahandler-helper.cpp
        src/cpp/media-rtppacketizationconfig-wrapper.cpp
        src/cpp/media-rtppacketizer-wrapper.cpp
        src/cpp/media-track-wrapper.cpp
        src/cpp/media-audio-wrapper.cpp
        src/cpp/media-video-wrapper.cpp
    )
endif()

if(NOT NO_WEBSOCKET)
    list(APPEND SRC_FILES
        src/cpp/web-socket-wrapper.cpp
        src/cpp/web-socket-server-wrapper.cpp
    )
endif()

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

execute_process(
    COMMAND node -p "require('node-addon-api').include_dir"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE NODE_ADDON_API_RESULT
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT NODE_ADDON_API_RESULT EQUAL 0 OR NOT NODE_ADDON_API_DIR)
    message(FATAL_ERROR "Failed to locate node-addon-api include_dir via node")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${NODE_ADDON_API_DIR}
    ${libdatachannel_SOURCE_DIR}/include
    ${libdatachannel_SOURCE_DIR}/deps/plog
)

set(LINK_LIBRARIES
    ${CMAKE_JS_LIB}
    datachannel-static
    plog::plog
)

if(APPLE)
    # Hide internal symbols (including statically linked OpenSSL) to avoid collisions in Electron/Node
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,-exported_symbol,_napi_register_module_v1
        -Wl,-exported_symbol,_node_api_module_get_api_version_v1
    )
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Android NDK uses Clang + libc++; avoid libgcc/libstdc++ flags
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--exclude-libs,ALL)
elseif(UNIX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND LINK_LIBRARIES -static-libgcc -static-libstdc++)
    endif()
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--exclude-libs,ALL)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    list(APPEND LINK_LIBRARIES crypt32)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBRARIES})

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
    # Generate node.lib
    separate_arguments(NODE_LIB_FLAGS NATIVE_COMMAND "${CMAKE_STATIC_LINKER_FLAGS}")
    execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${NODE_LIB_FLAGS})
endif()

include_guard(GLOBAL)

include(FetchContent)

# nlohmann/json: widely used modern JSON library with CMake package targets.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# pugixml: fast, lightweight XML parser and XPath engine.
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PUGIXML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.14
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# spdlog: structured and high-performance logging for services.
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# cpp-httplib: keep as header-only and use the exported httplib::httplib target.
set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.16.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# GoogleTest/GoogleMock: test framework for unit and integration checks.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(
    nlohmann_json
    pugixml
    spdlog
    httplib
    googletest
)

# Work around MemorySanitizer incompatibilities in gtest/gmock internals when msan is enabled.
set(_xmljson_global_flags "${CMAKE_CXX_FLAGS}")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(APPEND _xmljson_global_flags " ${CMAKE_CXX_FLAGS_${_cfg}}")
endforeach()
string(FIND "${_xmljson_global_flags}" "-fsanitize=memory" _xmljson_msan_pos)

if(NOT _xmljson_msan_pos EQUAL -1)
    if(TARGET gtest)
        target_compile_options(gtest PRIVATE -fno-sanitize=memory)
    endif()
    if(TARGET gmock)
        target_compile_options(gmock PRIVATE -fno-sanitize=memory)
    endif()
endif()
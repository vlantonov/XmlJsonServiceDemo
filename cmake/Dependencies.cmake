include_guard(GLOBAL)

find_package(nlohmann_json REQUIRED)
find_package(pugixml REQUIRED)
find_package(spdlog REQUIRED)
find_package(httplib REQUIRED)
find_package(GTest REQUIRED)

# Work around MemorySanitizer incompatibilities in gtest/gmock internals when msan is enabled.
set(_xmljson_global_flags "${CMAKE_CXX_FLAGS}")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(APPEND _xmljson_global_flags " ${CMAKE_CXX_FLAGS_${_cfg}}")
endforeach()
string(FIND "${_xmljson_global_flags}" "-fsanitize=memory" _xmljson_msan_pos)

if(NOT _xmljson_msan_pos EQUAL -1)
    if(TARGET GTest::gtest)
        target_compile_options(GTest::gtest INTERFACE -fno-sanitize=memory)
    endif()
    if(TARGET GTest::gmock)
        target_compile_options(GTest::gmock INTERFACE -fno-sanitize=memory)
    endif()
endif()
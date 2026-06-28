cmake_minimum_required(VERSION 3.14)

project(XmlJsonServiceDemo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(XMLJSON_BUILD_TESTS "Build unit tests" ON)
option(XMLJSON_BUILD_APP "Build the service binary" ON)
option(XMLJSON_ENABLE_COVERAGE "Enable lcov code coverage target" OFF)
option(XMLJSON_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(XMLJSON_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(Dependencies)
include(CompileOptions)
include(CodeCoverage)

add_library(xmljson STATIC
    lib/src/error.cpp
    lib/src/xml_to_json.cpp
    lib/src/json_to_xml.cpp
    lib/src/converter.cpp
    lib/src/config.cpp
    lib/src/logging.cpp
    lib/src/http_server.cpp
)

target_include_directories(xmljson
    PUBLIC
        lib/include
)

target_link_libraries(xmljson
    PUBLIC
        nlohmann_json::nlohmann_json
        pugixml::pugixml
        spdlog::spdlog
        httplib::httplib
)

xmljson_set_warnings(xmljson)
xmljson_apply_sanitizers(xmljson)
xmljson_enable_coverage(xmljson)

if(XMLJSON_BUILD_APP)
    add_executable(xmljson-service app/main.cpp)
    target_link_libraries(xmljson-service PRIVATE xmljson)
    xmljson_set_warnings(xmljson-service)
    xmljson_apply_sanitizers(xmljson-service)
endif()

enable_testing()

if(XMLJSON_BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        include(GoogleTest)

        add_executable(xmljson_tests
            tests/main.cpp
            tests/test_xml_to_json.cpp
            tests/test_json_to_xml.cpp
            tests/test_round_trip.cpp
            tests/test_config.cpp
            tests/test_logging.cpp
            tests/test_http_handlers.cpp
        )

        target_link_libraries(xmljson_tests
            PRIVATE
                xmljson
                GTest::gtest
                GTest::gmock
        )

        xmljson_set_warnings(xmljson_tests)
        xmljson_apply_sanitizers(xmljson_tests)
        gtest_discover_tests(xmljson_tests)

        add_executable(xmljson_integration_tests
            tests/main.cpp
            tests/integration/test_http_server.cpp
        )

        target_link_libraries(xmljson_integration_tests
            PRIVATE
                xmljson
                GTest::gtest
                GTest::gmock
        )

        xmljson_set_warnings(xmljson_integration_tests)
        xmljson_apply_sanitizers(xmljson_integration_tests)

        gtest_discover_tests(
            xmljson_integration_tests
            TEST_LIST XMLJSON_INTEGRATION_TESTS
            PROPERTIES LABELS integration
        )

        if(XMLJSON_INTEGRATION_TESTS)
            set_tests_properties(${XMLJSON_INTEGRATION_TESTS} PROPERTIES LABELS integration)
        endif()
    endif()
endif()
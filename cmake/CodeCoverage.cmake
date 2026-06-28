include_guard(GLOBAL)

function(xmljson_enable_coverage target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "xmljson_enable_coverage: target '${target}' does not exist")
    endif()

    if(XMLJSON_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -O0 -g --coverage)
        target_link_options(${target} PRIVATE --coverage)
    endif()
endfunction()

if(XMLJSON_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "XMLJSON_ENABLE_COVERAGE requires GCC or Clang")
    endif()

    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)

    if(NOT LCOV_EXECUTABLE)
        message(FATAL_ERROR "lcov not found. Install lcov to use XMLJSON_ENABLE_COVERAGE.")
    endif()

    if(NOT GENHTML_EXECUTABLE)
        message(FATAL_ERROR "genhtml not found. Install lcov/genhtml to use XMLJSON_ENABLE_COVERAGE.")
    endif()

    add_custom_target(cov
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV_EXECUTABLE} --capture --directory . --output-file coverage.info
        COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' '*/_deps/*' '*/tests/*' --output-file coverage.info
        COMMAND ${GENHTML_EXECUTABLE} coverage.info --output-directory cov
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report in build/cov"
        VERBATIM
    )
endif()
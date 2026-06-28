include_guard(GLOBAL)

option(XMLJSON_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

function(xmljson_set_warnings target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "xmljson_set_warnings: target '${target}' does not exist")
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(XMLJSON_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wnon-virtual-dtor
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )
        if(XMLJSON_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(xmljson_apply_sanitizers target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "xmljson_apply_sanitizers: target '${target}' does not exist")
    endif()

    if(MSVC)
        return()
    endif()

    set(_xmljson_sanitize_flags)

    if(XMLJSON_ENABLE_ASAN)
        list(APPEND _xmljson_sanitize_flags -fsanitize=address -fno-omit-frame-pointer)
    endif()

    if(XMLJSON_ENABLE_UBSAN)
        list(APPEND _xmljson_sanitize_flags -fsanitize=undefined)
    endif()

    if(_xmljson_sanitize_flags)
        list(REMOVE_DUPLICATES _xmljson_sanitize_flags)
        target_compile_options(${target} PRIVATE ${_xmljson_sanitize_flags})
        target_link_options(${target} PRIVATE ${_xmljson_sanitize_flags})
    endif()
endfunction()
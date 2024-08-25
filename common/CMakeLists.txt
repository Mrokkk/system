function(add_common_library name name_prefix make_alias)

    file(GLOB_RECURSE COMMON_SRC "${PROJECT_SOURCE_DIR}/common/*.c")

    add_library(${name} OBJECT
        ${COMMON_SRC}
    )

    target_include_directories(${name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_definitions(${name}
        PRIVATE
            NAME_PREFIX=${name_prefix}
            ARCH=${ARCH}
    )

    if(make_alias)
        target_compile_definitions(${name}
            PRIVATE
            MAKE_ALIAS
        )
    endif()

endfunction()

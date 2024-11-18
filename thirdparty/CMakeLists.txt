find_package(Git REQUIRED)

function(find_target_output output_var target)
    if (CMAKE_BUILD_TYPE)
        set(build_suffix _${CMAKE_BUILD_TYPE})
    else()
        set(build_path_suffix Release/)
    endif()

    get_target_property(filename ${target} OUTPUT_NAME${build_suffix})
    if (NOT filename)
        get_target_property(filename ${target} NAME)
    endif()

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        get_target_property(debug_postfix ${target} DEBUG_POSTFIX)
    endif()

    if (MSVC)
        set(extension ".lib")
    else()
        set(extension ".a")
        get_property(filename_prefix TARGET ${target} PROPERTY PREFIX)
    endif()

    get_target_property(path ${target} BINARY_DIR)

    set(${output_var} ${path}/${build_path_suffix}${filename_prefix}${filename}${debug_postfix}${extension} PARENT_SCOPE)
endfunction()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(SKIP_INSTALL_ALL TRUE)
set(ENABLE_BZIP2 OFF)
set(ENABLE_LZMA OFF)

# rapidxml Library
add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rapidxml)

# zlib Library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib EXCLUDE_FROM_ALL)
# TODO better handling of ZLIB_LIBRARY
find_target_output(ZLIB_LIBRARY zlibstatic)
set(ZLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
find_package(ZLIB REQUIRED)
target_include_directories(ZLIB::ZLIB INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/zlib)
add_dependencies(ZLIB::ZLIB zlibstatic)

# libpng Library
set(PNG_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libpng EXCLUDE_FROM_ALL)

# libzip Library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libzip EXCLUDE_FROM_ALL)

# cxxopts Library
add_library(cxxopts INTERFACE)
target_include_directories(cxxopts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts/include)

if(SHADY_ENABLE_MODULE)
    # libcurl Library
    set(CURL_USE_SCHANNEL ON)
    set(CURL_USE_LIBSSH2 OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/curl EXCLUDE_FROM_ALL)

    # nlohmann JSON Library
    add_library(json INTERFACE)
    target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/json/include)

    # Lua Library
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lua-cmake EXCLUDE_FROM_ALL)
    set(LUABRIDGE_TESTING OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge EXCLUDE_FROM_ALL)

    # SokuLib Library
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/SokuLib EXCLUDE_FROM_ALL)
endif()

# if(SHADY_ENABLE_TEST)
#     # gtest Library
#     add_git_module(googletest https://github.com/google/googletest release-1.11.0)
#     add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest EXCLUDE_FROM_ALL)

#     # benchmark Library
#     add_git_module(benchmark https://github.com/google/benchmark v1.6.0)
#     add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark EXCLUDE_FROM_ALL)
# endif()

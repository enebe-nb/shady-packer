find_package(Git REQUIRED)

macro(add_git_module target uri branch)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${target})
		message(STATUS "Cloning ${target} from ${uri}")
		execute_process(COMMAND ${GIT_EXECUTABLE} clone --branch ${branch} --depth 1
            ${uri} ${CMAKE_CURRENT_SOURCE_DIR}/${target} ERROR_QUIET)
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${target})
            message(FATAL_ERROR "Dependency error: Failed to download ${target}")
        endif()
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1 ${ARGN}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${target})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${target}.patch)
            execute_process(COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/${target}.patch
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${target})
        endif()
    endif()
endmacro()

macro(add_download_module target suffix uri)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${target})
		message(STATUS "Downloading ${target} from ${uri}")
        set(path ${CMAKE_CURRENT_SOURCE_DIR}/${target})
        file(DOWNLOAD ${uri} ${path}${suffix}.zip)
        if(NOT EXISTS ${path}${suffix}.zip)
            message(FATAL_ERROR "Dependency error: Failed to download ${target}")
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${path}${suffix}.zip" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" )
        file(RENAME "${path}${suffix}" "${path}")
        file(REMOVE "${path}${suffix}.zip")
    endif()
endmacro()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(SKIP_INSTALL_ALL TRUE)
set(ENABLE_BZIP2 OFF)
set(ENABLE_LZMA OFF)

# rapidxml Library
add_download_module(rapidxml "-1.13" https://downloads.sourceforge.net/project/rapidxml/rapidxml/rapidxml%201.13/rapidxml-1.13.zip)
add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rapidxml)

# zlib Library
add_git_module(zlib git://github.com/madler/zlib.git v1.2.11)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib EXCLUDE_FROM_ALL)
if (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/zlib/Release/zlibstatic.lib)
else()
    set(ZLIB_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/zlib/libzlibstatic.a)
endif()
set(ZLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*" "\\1" ZLIB_VERSION_STRING ${_zlib_h_contents})
find_package(ZLIB REQUIRED)
set_target_properties(ZLIB::ZLIB PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS};${CMAKE_CURRENT_BINARY_DIR}/zlib")
add_dependencies(ZLIB::ZLIB zlibstatic)

# libpng Library
add_git_module(libpng git://git.code.sf.net/p/libpng/code libpng16)
set(PNG_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libpng EXCLUDE_FROM_ALL)
target_include_directories(png_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libpng ${CMAKE_CURRENT_BINARY_DIR}/zlib ${CMAKE_CURRENT_BINARY_DIR}/libpng)

# libzip Library
add_git_module(libzip git://github.com/nih-at/libzip v1.8.0)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libzip EXCLUDE_FROM_ALL)
#target_compile_definitions(zip INTERFACE ZIP_STATIC)

# cxxopts Library
add_git_module(cxxopts git://github.com/jarro2783/cxxopts.git v2.2.1)
add_library(cxxopts INTERFACE)
target_include_directories(cxxopts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts/include)

if(SHADY_ENABLE_MODULE)
    # libcurl Library
    add_download_module(curl "-7.78.0" https://curl.haxx.se/download/curl-7.78.0.zip)
    set(CMAKE_USE_SCHANNEL ON)
    set(CMAKE_USE_LIBSSH2 OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/curl EXCLUDE_FROM_ALL)

    # nlohmann JSON Library
    add_git_module(json git://github.com/nlohmann/json v3.10.2)
    add_library(json INTERFACE)
    target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/json/include)

    # Lua Library
    add_git_module(lua git://github.com/lua/lua v5.4.3)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lua EXCLUDE_FROM_ALL)
    add_git_module(LuaBridge git://github.com/vinniefalco/LuaBridge 2.6)
    set(LUABRIDGE_TESTING OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge EXCLUDE_FROM_ALL)

    # termcolor Library
#    add_git_module(termcolor git://github.com/ikalnytskyi/termcolor v2.0.0)
#    add_library(termcolor INTERFACE)
#    target_include_directories(termcolor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/termcolor/include)

    # SokuLib Library
    add_git_module(SokuLib git://github.com/enebe-nb/SokuLib dev)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/SokuLib EXCLUDE_FROM_ALL)
endif()

if(SHADY_ENABLE_TEST)
    # gtest Library
    add_git_module(googletest git://github.com/google/googletest release-1.11.0)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest EXCLUDE_FROM_ALL)
endif()

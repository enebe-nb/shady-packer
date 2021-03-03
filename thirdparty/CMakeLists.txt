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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libpng EXCLUDE_FROM_ALL)
target_include_directories(png_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libpng ${CMAKE_CURRENT_BINARY_DIR}/zlib ${CMAKE_CURRENT_BINARY_DIR}/libpng)

# libzip Library
add_git_module(libzip git://github.com/nih-at/libzip.git v1.7.3)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libzip EXCLUDE_FROM_ALL)
#target_compile_definitions(zip INTERFACE ZIP_STATIC)

if(SHADY_ENABLE_MODULE)
    # libcurl Library
    add_download_module(curl "-7.75.0" https://curl.haxx.se/download/curl-7.75.0.zip)
    set(CMAKE_USE_SCHANNEL ON)
    set(CMAKE_USE_LIBSSH2 OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/curl EXCLUDE_FROM_ALL)

    # nlohmann JSON Library
    add_git_module(json https://github.com/nlohmann/json v3.9.1)
    add_library(json INTERFACE)
    target_include_directories(json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/json/include)

    # Lua Library
    add_git_module(lua https://github.com/lua/lua v5.4.2)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lua EXCLUDE_FROM_ALL)
    add_git_module(LuaBridge https://github.com/vinniefalco/LuaBridge 2.6)
    set(LUABRIDGE_TESTING OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge EXCLUDE_FROM_ALL)

    # termcolor Library
    add_git_module(termcolor https://github.com/ikalnytskyi/termcolor v1.0.1)
    add_library(termcolor INTERFACE)
    target_include_directories(termcolor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/termcolor/include)
endif()

if(SHADY_ENABLE_CLI OR SHADY_ENABLE_MODULE)
    # cxxopts Library
    add_git_module(cxxopts git://github.com/jarro2783/cxxopts.git v1.4.4)
    add_library(cxxopts INTERFACE)
    target_include_directories(cxxopts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts/include)
endif()

if(SHADY_ENABLE_TEST)
    # cxxtest Library
    enable_testing()
    add_git_module(cxxtest git://github.com/cxxtest/cxxtest 4.4)
    set(CXXTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/cxxtest PARENT_SCOPE)
endif()

if(SHADY_ENABLE_GUI)
    # rtaudio Library
    if(APPLE)
        # TODO Mac OSX Core Library
    elseif(UNIX)
        set(ALSA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/alsa-lib)
        if(NOT EXISTS ${ALSA_ROOT})
            add_git_module(alsa-lib git://alsa-project.org/alsa-lib.git v1.1.5)
            execute_process(COMMAND ${ALSA_ROOT}/gitcompile WORKING_DIRECTORY ${ALSA_ROOT})
        endif()
        set(ALSA_INCLUDE_DIR ${ALSA_ROOT}/include)
        set(ALSA_LIBRARY ${ALSA_ROOT}/src/.libs/libasound.so)
    endif()
    add_git_module(rtaudio git://github.com/thestk/rtaudio.git v5.0.0)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rtaudio EXCLUDE_FROM_ALL)
    set(RTAUDIO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rtaudio PARENT_SCOPE)

    # gtkmm Library
    find_package(PkgConfig REQUIRED)
    if(PKGCONFIG_FOUND)
        pkg_check_modules(GTK3 gtkmm-3.0)
        find_package(Threads)
        set(GTK3_FOUND ${GTK3_FOUND} PARENT_SCOPE)
        set(GTK3_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS} PARENT_SCOPE)
        set(GTK3_LIBRARIES ${GTK3_LIBRARIES} PARENT_SCOPE)
        set(CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT} PARENT_SCOPE)
    else()
        #TODO gtkmm Build Tree
    endif()
endif()

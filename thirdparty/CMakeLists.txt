find_package(Git REQUIRED)

macro(add_git_module target uri branch)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/${target})
		message(STATUS "Cloning ${target} from ${uri}")
		execute_process(COMMAND ${GIT_EXECUTABLE} clone --branch ${branch} --depth 1
            ${uri} ${CMAKE_SOURCE_DIR}/thirdparty/${target})
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1 ${ARGN}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/${target})
        if(EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/${target}.patch)
            execute_process(COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_SOURCE_DIR}/thirdparty/${target}.patch
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/${target})
        endif()
    endif()
endmacro()

# Boost Library
add_git_module(boost git://github.com/boostorg/boost.git boost-1.65.1)
include (${CMAKE_SOURCE_DIR}/thirdparty/boost.cmake)
set(BOOST_INCLUDE_DIR ${BOOST_INCLUDE_DIR} PARENT_SCOPE)
set(BOOST_LIBRARY_DIR ${BOOST_BINARY_DIR}/lib PARENT_SCOPE)
set(BOOST_LIBRARIES ${BOOST_LIBRARIES} PARENT_SCOPE)

# rapidxml Library
set(RAPIDXML_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/rapidxml)
if(NOT EXISTS ${RAPIDXML_ROOT})
	message(STATUS "Downloading rapidxml from ${RAPIDXML_URI}")
    set(RAPIDXML_URI https://downloads.sourceforge.net/project/rapidxml/rapidxml/rapidxml%201.13/rapidxml-1.13.zip)
    file(DOWNLOAD ${RAPIDXML_URI} ${RAPIDXML_ROOT}-1.13.zip)
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz ${RAPIDXML_ROOT}-1.13.zip WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty" )
    file(RENAME ${RAPIDXML_ROOT}-1.13 ${RAPIDXML_ROOT})
    file(REMOVE ${RAPIDXML_ROOT}-1.13.zip)
endif()
set(RAPIDXML_INCLUDE_DIR ${RAPIDXML_ROOT} PARENT_SCOPE)

# zlib Library
add_git_module(zlib git://github.com/madler/zlib.git v1.2.11)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/zlib EXCLUDE_FROM_ALL)
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/zlib ${CMAKE_BINARY_DIR}/thirdparty/zlib)
file(READ ${CMAKE_SOURCE_DIR}/thirdparty/zlib/zlib.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*" "\\1" ZLIB_VERSION_STRING ${_zlib_h_contents})

# libpng Library
set(DISABLE_SYMLINK FALSE CACHE BOOL "Disable use of symlink")
set(SKIP_INSTALL_EXPORT TRUE) # TODO install
add_git_module(libpng git://git.code.sf.net/p/libpng/code libpng16)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/libpng EXCLUDE_FROM_ALL)
set(LIBPNG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/libpng ${CMAKE_BINARY_DIR}/thirdparty/libpng PARENT_SCOPE)
add_dependencies(png_static zlibstatic)

# libzip Library
set(DISABLE_BZIP TRUE CACHE BOOL "Disable use of bzip library")
add_git_module(libzip git://github.com/nih-at/libzip.git rel-1-3-0)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/libzip EXCLUDE_FROM_ALL)
set(LIBZIP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/libzip/lib ${CMAKE_BINARY_DIR}/thirdparty/libzip PARENT_SCOPE)
target_compile_definitions(zipstatic INTERFACE ZIP_STATIC)
add_dependencies(zipstatic zlibstatic)

set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIR} PARENT_SCOPE)

if(ENABLE_CLI)
    # cxxopts Library
    add_git_module(cxxopts git://github.com/jarro2783/cxxopts.git v1.4.4)
    set(CXXOPTS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/cxxopts/include PARENT_SCOPE)
endif()

if(ENABLE_TEST)
    # cxxtest Library
    enable_testing()
    add_git_module(cxxtest git://github.com/cxxtest/cxxtest 4.4)
    set(CXXTEST_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/cxxtest PARENT_SCOPE)
endif()

if(ENABLE_GUI)
    # rtaudio Library
    if(APPLE)
        # TODO Mac OSX Core Library
    elseif(UNIX)
        set(ALSA_ROOT ${CMAKE_SOURCE_DIR}/thirdparty/alsa-lib)
        if(NOT EXISTS ${ALSA_ROOT})
            add_git_module(alsa-lib git://alsa-project.org/alsa-lib.git v1.1.5)
            execute_process(COMMAND ${ALSA_ROOT}/gitcompile WORKING_DIRECTORY ${ALSA_ROOT})
        endif()
        set(ALSA_INCLUDE_DIR ${ALSA_ROOT}/include)
        set(ALSA_LIBRARY ${ALSA_ROOT}/src/.libs/libasound.so)
    endif()
    add_git_module(rtaudio git://github.com/thestk/rtaudio.git v5.0.0)
    add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/rtaudio EXCLUDE_FROM_ALL)
    set(RTAUDIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/rtaudio PARENT_SCOPE)

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

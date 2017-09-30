cmake_minimum_required(VERSION 2.6)
project(shady-packer)

# Boost Library
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS filesystem thread program_options locale)
include_directories(${Boost_INCLUDE_DIRS})

# LIBPNG Library
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})

# libzip Library
find_path(LIBZIP_INCLUDE_DIR_ZIP NAMES zip.h HINTS ${PC_LIBZIP_INCLUDE_DIRS})
find_path(LIBZIP_INCLUDE_DIR_ZIPCONF NAMES zipconf.h HINTS ${PC_LIBZIP_INCLUDE_DIRS})
find_library(LIBZIP_LIBRARY NAMES zip)
if (NOT LIBZIP_LIBRARY)
	message(FATAL_ERROR "Libzip not Found")
endif (NOT LIBZIP_LIBRARY)
include_directories(${LIBZIP_INCLUDE_DIR_ZIP} ${LIBZIP_INCLUDE_DIR_ZIPCONF})

# SDL2 Library
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/thirdparty/sdl2-cmake")
find_package(SDL2)
find_package(OpenGL)

# CxxTest Library
find_package(CxxTest)

# Submodules Linking
include_directories("${CMAKE_SOURCE_DIR}/thirdparty/rapidxml")
include_directories("${CMAKE_SOURCE_DIR}/thirdparty/nuklear")
include_directories("${CMAKE_SOURCE_DIR}/thirdparty/tinyfiledialogs")
set(TINYFILEDIALOGS_SOURCE "${CMAKE_SOURCE_DIR}/thirdparty/tinyfiledialogs/tinyfiledialogs.c")

# Shady *Core*
set(SRC_CORE_Resource
	src/Core/resource/image.cpp
	src/Core/resource/gui.cpp
	src/Core/resource/pattern.cpp
	src/Core/resource/sfx.cpp
	src/Core/resource/text.cpp
	src/Core/resource/readerwriter.hpp
	src/Core/resource/readerwriter.cpp )
source_group("Source Files\\resource" FILES ${SRC_CORE_Resource})

set(SRC_CORE_Util
	src/Core/util/iohelper.hpp
    src/Core/util/riffdocument.hpp
    src/Core/util/singleton.hpp
    src/Core/util/strallocator.hpp
	src/Core/util/xmlprinter.hpp
    src/Core/util/util.cpp )
source_group("Source Files\\util" FILES ${SRC_CORE_Util})

set(SRC_CORE_Root
	src/Core/baseentry.hpp
	src/Core/dataentry.hpp
	src/Core/dataentry.cpp
	src/Core/fileentry.hpp
	src/Core/fileentry.cpp
	src/Core/zipentry.hpp
	src/Core/zipentry.cpp

	src/Core/resource.hpp
	src/Core/package.hpp
	src/Core/package.cpp
	src/Core/packagefilter.cpp )
source_group("Source Files" FILES ${SRC_CORE_Root})

add_library(shady-packer-core STATIC ${SRC_CORE_Resource} ${SRC_CORE_Util} ${SRC_CORE_Root})
target_link_libraries(shady-packer-core ${Boost_LIBRARIES} ${PNG_LIBRARIES} ${LIBZIP_LIBRARY})
target_compile_definitions(shady-packer-core PRIVATE
	USEWINDOWS_CALLBACK
)

# Shady *Cli*
set(SRC_CLI
	src/Cli/command.hpp
	src/Cli/command.cpp
)
source_group("Source Files" FILES ${SRC_CLI})
add_executable(shady-packer-cli ${SRC_CLI})
target_link_libraries(shady-packer-cli shady-packer-core)

# Shady *Gui*
if(SDL2_FOUND AND OPENGL_FOUND)
    include_directories(${SDL2_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})

	set(SRC_GUI_Root
        src/Gui/application.hpp
        src/Gui/application.cpp
        src/Gui/renderer.hpp
        src/Gui/renderer.cpp

        src/Gui/window.hpp
        src/Gui/window.cpp
        src/Gui/dialog.hpp
        src/Gui/dialog.cpp
        src/Gui/editor.hpp
        src/Gui/editor.cpp
    )
	source_group("Source Files" FILES ${SRC_GUI_Root})

	set(SRC_GUI_Lib
        src/Gui/nuklear.cpp
        ${TINYFILEDIALOGS_SOURCE}
	)
	source_group("Library Files" FILES ${SRC_GUI_Lib})

    add_executable(shady-packer WIN32 ${SRC_GUI_Root} ${SRC_GUI_Lib})
    target_link_libraries(shady-packer shady-packer-core ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY})
    target_compile_definitions(shady-packer PRIVATE
        NK_INCLUDE_FIXED_TYPES
        #NK_INCLUDE_STANDARD_VARARGS
        NK_INCLUDE_STANDARD_IO
        NK_INCLUDE_DEFAULT_ALLOCATOR
        NK_INCLUDE_VERTEX_BUFFER_OUTPUT
        NK_INCLUDE_FONT_BAKING
        NK_INCLUDE_DEFAULT_FONT
    )
endif(SDL2_FOUND AND OPENGL_FOUND)

# Shady *Test*
if(CxxTest_FOUND)
    include_directories(${CXXTEST_INCLUDE_DIR})
    enable_testing()

    CXXTEST_ADD_TEST(shady-test test.cpp
        ${CMAKE_SOURCE_DIR}/src/Test/readerwriter.hpp
        ${CMAKE_SOURCE_DIR}/src/Test/dataentry.hpp
        ${CMAKE_SOURCE_DIR}/src/Test/fileentry.hpp
        ${CMAKE_SOURCE_DIR}/src/Test/package.hpp
        ${CMAKE_SOURCE_DIR}/src/Test/util.hpp
        ${CMAKE_SOURCE_DIR}/src/Test/zipentry.hpp
    )
    target_link_libraries(shady-test shady-packer-core)
    add_custom_command(TARGET shady-test POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS -E copy_directory ${CMAKE_SOURCE_DIR}/src/Test/data test-data
    )
endif(CxxTest_FOUND)

# Shady *Loader*
if(WIN32)
	add_library(shady-loader MODULE src/Loader/loader.cpp)
	target_link_libraries(shady-loader ${LIBZIP_LIBRARY})
endif(WIN32)
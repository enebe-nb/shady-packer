cmake_minimum_required(VERSION 2.6)
project(shady-packer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENABLE_CLI TRUE CACHE BOOL "Enable command-line building")
set(ENABLE_GUI TRUE CACHE BOOL "Enable gui building")
set(ENABLE_TEST FALSE CACHE BOOL "Enable test building")

# Dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty)

# Shady *Core*
add_subdirectory(${CMAKE_SOURCE_DIR}/src/Core)

# Shady *Loader* (only make sense to build in windows 32bits)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Loader)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Lua)
endif()

# Shady *Cli*
if(ENABLE_CLI)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/Cli)
endif()

# Shady *Gui*
if(ENABLE_GUI)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Gui)
endif()

# Shady *Test*
if(ENABLE_TEST)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Test)
endif()

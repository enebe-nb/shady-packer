cmake_minimum_required(VERSION 3.16)
project(shady-packer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHADY_ENABLE_CLI TRUE CACHE BOOL "Enable command-line building")
set(SHADY_ENABLE_TEST FALSE CACHE BOOL "Enable test building")
if (MSVC)
	set(SHADY_ENABLE_GUI FALSE CACHE BOOL "Enable gui building")
	set(SHADY_ENABLE_MODULE TRUE CACHE BOOL "Enable module building")
else()
	set(SHADY_ENABLE_GUI TRUE CACHE BOOL "Enable gui building")
	set(SHADY_ENABLE_MODULE FALSE CACHE BOOL "Enable module building")
endif()

# Dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty)

# Shady *Core*
add_subdirectory(${CMAKE_SOURCE_DIR}/src/Core)

# Shady *Loader*
if(SHADY_ENABLE_MODULE)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Loader)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Lua)
endif()

# Shady *Cli*
if(SHADY_ENABLE_CLI)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/Cli)
endif()

# Shady *Gui*
if(SHADY_ENABLE_GUI)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Gui)
endif()

# Shady *Test*
if(SHADY_ENABLE_TEST)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Test)
endif()

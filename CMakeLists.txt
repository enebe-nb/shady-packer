cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0091 NEW)

string(TIMESTAMP SHADY_VER_DAY "%m%d" UTC)
string(TIMESTAMP SHADY_VER_YEAR "%Y" UTC)
project(shady-packer
	VERSION 1.3.${SHADY_VER_YEAR}.${SHADY_VER_DAY}
	DESCRIPTION "Tools for a specific fighting game.")
set(SHADY_VERSION ${PROJECT_VERSION})
string(REPLACE . , SHADY_VERSION_COMMA ${SHADY_VERSION})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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

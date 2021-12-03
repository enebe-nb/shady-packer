cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0091 NEW)

string(TIMESTAMP SHADY_VER_DAY "%m%d" UTC)
string(TIMESTAMP SHADY_VER_YEAR "%Y" UTC)
project(shady-packer
	VERSION 2.0.${SHADY_VER_YEAR}.${SHADY_VER_DAY}
	DESCRIPTION "Tools for a specific fighting game.")
set(SHADY_VERSION ${PROJECT_VERSION})
string(REPLACE . , SHADY_VERSION_COMMA ${SHADY_VERSION})

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(SHADY_ENABLE_TEST FALSE CACHE BOOL "Enable test building")
set(SHADY_ENABLE_MODULE TRUE CACHE BOOL "Enable module building")

# Dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty)

# Shady *Core*
add_subdirectory(${CMAKE_SOURCE_DIR}/src/Core)
# Shady *Cli*
add_subdirectory(${CMAKE_SOURCE_DIR}/src/Cli)

# Shady *Loader*
if(SHADY_ENABLE_MODULE)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Loader)
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Lua)
endif()

# Shady *Test*
if(SHADY_ENABLE_TEST)
	enable_testing()
	add_subdirectory(${CMAKE_SOURCE_DIR}/src/Test)
endif()

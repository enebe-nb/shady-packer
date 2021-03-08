set(CMAKE_STATIC_LIBRARY_PREFIX "")

set(SRC_LOADER_Common
	modpackage.cpp
	asynctask.cpp
)
source_group("Source Files\\Common" FILES ${SRC_LOADER_Common})

set(SRC_LOADER_Loader
	main.cpp
	menu.cpp
	converter.cpp
	integration.cpp
)
source_group("Source Files\\Loader" FILES ${SRC_LOADER_Loader})

set(SRC_LOADER_Manage
	manager.cpp
	${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)
source_group("Source Files\\Manage" FILES ${SRC_LOADER_Manage})

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(WARNING Skipping shady-loader: Can only build on Windows 32bits)
else()
    add_library(shady-loader MODULE ${SRC_LOADER_Common} ${SRC_LOADER_Loader})
    target_link_libraries(shady-loader -static shady-core shady-lualib libcurl json)
	set_target_properties(shady-loader PROPERTIES PREFIX "")
    install(TARGETS shady-loader LIBRARY DESTINATION soku-modules)
endif()

if(WIN32)
	set(SHADY_TARGET_NAME "shady-manage")
	set(SHADY_NAME "Shady Manage")
	set(SHADY_DESCRIPTION "A specific fighting game package manager.")
	configure_file(../resource.rc.in resource.rc @ONLY)
	file(COPY ../icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_executable(shady-manage ${SRC_LOADER_Common} ${SRC_LOADER_Manage})
target_link_libraries(shady-manage -static libcurl termcolor json)

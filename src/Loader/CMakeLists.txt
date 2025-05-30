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

if(WIN32)
	set(SHADY_TARGET_NAME "shady-loader")
	set(SHADY_TARGET_FILE "shady-loader.dll")
	set(SHADY_NAME "Shady Loader")
	set(SHADY_ICON_STRING "")
	set(SHADY_FILETYPE "VFT_DLL")
	configure_file(../resource.rc.in resource.rc @ONLY)
	list(APPEND SRC_LOADER_Loader ${CMAKE_CURRENT_BINARY_DIR}/resource.rc)
endif()

add_library(shady-loader MODULE ${SRC_LOADER_Common} ${SRC_LOADER_Loader})
target_link_libraries(shady-loader -static shady-core shady-lualib libcurl json)
set_target_properties(shady-loader PROPERTIES PREFIX "")
install(TARGETS shady-loader LIBRARY DESTINATION modules/shady-loader)

if(SHADY_ENABLE_EXECUTABLE)
	file(GLOB SRC_LOADER_Pack ${CMAKE_CURRENT_SOURCE_DIR}/data/*)
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat"
		COMMAND $<TARGET_FILE:shady-cli> pack -m data
			-o "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat"
			"${CMAKE_CURRENT_SOURCE_DIR}/data"
		DEPENDS ${SRC_LOADER_Pack}
		VERBATIM
	)
	add_custom_target(shady-loader-pack DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat" shady-cli)
	add_dependencies(shady-loader shady-loader-pack)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat" DESTINATION modules/shady-loader)
endif()

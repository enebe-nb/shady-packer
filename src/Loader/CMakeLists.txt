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

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(WARNING Skipping shady-loader: Can only build on Windows 32bits)
else()
	target_compile_definitions(shady-core PUBLIC $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)
	target_compile_definitions(gtest PUBLIC $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)
	target_compile_definitions(benchmark PUBLIC $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)

	add_library(shady-loader MODULE ${SRC_LOADER_Common} ${SRC_LOADER_Loader})
    target_link_libraries(shady-loader -static shady-core shady-lualib libcurl json)
	set_target_properties(shady-loader PROPERTIES PREFIX "")
    install(TARGETS shady-loader LIBRARY DESTINATION soku-modules)

	file(GLOB SRC_LOADER_Pack ${CMAKE_CURRENT_SOURCE_DIR}/data/*)
	# add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat"
	# 	COMMAND $<TARGET_FILE:shady-cli> pack -m data
	# 		-o "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat"
	# 		"${CMAKE_CURRENT_SOURCE_DIR}/data"
	# 	DEPENDS ${SRC_LOADER_Pack}
	# 	VERBATIM
	# )
	# add_custom_target(shady-loader-pack DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat" shady-cli)
	# add_dependencies(shady-loader shady-loader-pack)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/shady-loader.dat" DESTINATION soku-modules)
endif()

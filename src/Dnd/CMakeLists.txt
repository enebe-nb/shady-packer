set(SRC_DND
	main.cpp
	${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)
source_group("Source Files" FILES ${SRC_DND})

if(WIN32)
	set(SHADY_TARGET_NAME "shady-dnd-pack")
	set(SHADY_NAME "Shady Drag'n'Drop Packer")
	set(SHADY_DESCRIPTION "A specific fighting game resource packer.")
	configure_file(../resource.rc.in resource.rc @ONLY)
	file(COPY ../icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_executable(shady-dnd-pack ${SRC_DND})
target_link_libraries(shady-dnd-pack shady-core)
install(TARGETS shady-dnd-pack RUNTIME DESTINATION bin)
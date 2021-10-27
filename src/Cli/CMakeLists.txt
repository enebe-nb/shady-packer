set(SRC_CLI
	command.hpp
	command.cpp
#	convert.cpp
	merge.cpp
	merge-palette.cpp
	pack.cpp
	scale.cpp
	${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)
source_group("Source Files" FILES ${SRC_CLI})

if(WIN32)
	set(SHADY_TARGET_NAME "shady-cli")
	set(SHADY_NAME "Shady Cli")
	set(SHADY_DESCRIPTION "A specific fighting game resource manipulator.")
	configure_file(../resource.rc.in resource.rc @ONLY)
	file(COPY ../icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_executable(shady-cli ${SRC_CLI})
target_link_libraries(shady-cli -static shady-core cxxopts)
install(TARGETS shady-cli RUNTIME DESTINATION bin)
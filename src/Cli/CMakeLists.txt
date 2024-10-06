set(SRC_CLI
	command.hpp
	command.cpp
	convert.cpp
	merge.cpp
	merge-palette.cpp
	pack.cpp
	scale.cpp
	list.cpp
)

if(WIN32)
	set(SHADY_TARGET_NAME "shady-cli")
	set(SHADY_NAME "Shady Cli")
	set(SHADY_DESCRIPTION "A specific fighting game resource manipulator.")
	configure_file(../resource.rc.in resource.rc @ONLY)
	list(APPEND SRC_CLI ${CMAKE_CURRENT_BINARY_DIR}/resource.rc)
	file(COPY ../icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

source_group("Source Files" FILES ${SRC_CLI})
add_executable(shady-cli ${SRC_CLI})
if (WIN32)
target_link_libraries(shady-cli -static shady-core cxxopts)
else()
target_link_libraries(shady-cli  shady-core cxxopts)
endif()
install(TARGETS shady-cli RUNTIME DESTINATION bin)
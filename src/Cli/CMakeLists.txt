set(SRC_CLI
	command.hpp
	command.cpp
	convert.cpp
	merge.cpp
	merge-palette.cpp
	merge-pattern.cpp
	pack.cpp
	scale.cpp
)
source_group("Source Files" FILES ${SRC_CLI})

add_executable(shady-cli ${SRC_CLI})
target_link_libraries(shady-cli -static shady-core cxxopts)
install(TARGETS shady-cli RUNTIME DESTINATION bin)
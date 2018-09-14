link_directories(${BOOST_LIBRARY_DIR})
include_directories(
    ${CXXOPTS_INCLUDE_DIR}
    ${LIBPNG_INCLUDE_DIR}
    ${RAPIDXML_INCLUDE_DIR}
    ${BOOST_INCLUDE_DIR}
)

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
target_link_libraries(shady-cli shady-core -static)
install(TARGETS shady-cli RUNTIME DESTINATION bin)
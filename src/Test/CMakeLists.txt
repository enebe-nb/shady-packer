list(APPEND CMAKE_PREFIX_PATH ${CXXTEST_ROOT})
find_package(CxxTest REQUIRED)
include_directories(${CXXTEST_INCLUDE_DIR})

CXXTEST_ADD_TEST(shady-test test.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/readerwriter.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/dataentry.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/fileentry.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/package.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/util.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/zipentry.hpp
)

target_link_libraries(shady-test -static shady-core)
add_custom_command(TARGET shady-test POST_BUILD
	COMMAND ${CMAKE_COMMAND}
	ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data test-data
)

install(TARGETS shady-test RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-data DESTINATION bin)
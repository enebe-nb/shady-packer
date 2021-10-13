set(SRC_TEST_Test
	readerwriter.cpp
	dataentry.cpp
	fileentry.cpp
	package.cpp
	util.hpp
	zipentry.cpp
)
source_group("Source Files\\Test" FILES ${SRC_TEST_Test})

add_executable(shady-test ${SRC_TEST_Test})
target_link_libraries(shady-test -static shady-core gtest_main)
add_test(NAME "ShadyTests" COMMAND shady-test WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(TARGET shady-test POST_BUILD
	COMMAND ${CMAKE_COMMAND}
	ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data test-data
)

install(TARGETS shady-test RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-data DESTINATION bin)
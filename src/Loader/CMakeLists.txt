link_directories(${BOOST_LIBRARY_DIR})
include_directories(${BOOST_INCLUDE_DIR})
include_directories(${LIBZIP_INCLUDE_DIR})
include_directories(${SOKULIB_INCLUDE_DIR})

add_library(shady-loader MODULE loader.cpp)
target_link_libraries(shady-loader zipstatic zlibstatic shlwapi -static ${SOKULIB_LIBRARIES} shady-core)
set_target_properties(shady-loader PROPERTIES PREFIX "")

install(TARGETS shady-loader LIBRARY DESTINATION shady-loader)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/shady-loader.ini DESTINATION shady-loader)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/th123e.zip DESTINATION shady-loader)

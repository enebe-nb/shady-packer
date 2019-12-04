include_directories(${SOKULIB_INCLUDE_DIR})

add_definitions(-DIMGUI_DISABLE_INCLUDE_IMCONFIG_H)
add_library(shady-loader MODULE loader.cpp)
target_link_libraries(shady-loader -static shlwapi ${SOKULIB_LIBRARIES})
set_target_properties(shady-loader PROPERTIES PREFIX "")

install(TARGETS shady-loader LIBRARY DESTINATION shady-loader)

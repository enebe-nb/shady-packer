link_directories(${BOOST_LIBRARY_DIR})
include_directories(
    ${CURL_INCLUDE_DIR}
    ${SOKULIB_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR})

add_definitions(-DIMGUI_DISABLE_INCLUDE_IMCONFIG_H)
add_library(shady-loader MODULE main.cpp menu.cpp converter.cpp modpackage.cpp asynctask.cpp decodehtml.cpp reader.cpp)
target_link_libraries(shady-loader zipstatic zlibstatic shlwapi libcurl -static ${SOKULIB_LIBRARIES} shady-core)
set_target_properties(shady-loader PROPERTIES PREFIX "")

install(TARGETS shady-loader LIBRARY DESTINATION soku-modules)

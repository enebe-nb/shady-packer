link_directories(${BOOST_LIBRARY_DIR})
include_directories(
    ${CURL_INCLUDE_DIR}
    ${SOKULIB_INCLUDE_DIR}
    ${CXXOPTS_INCLUDE_DIR}
    ${TERMCOLOR_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR})

add_definitions(-DIMGUI_DISABLE_INCLUDE_IMCONFIG_H)
add_library(shady-loader MODULE main.cpp menu.cpp converter.cpp modpackage.cpp asynctask.cpp reader.cpp)
target_link_libraries(shady-loader zipstatic zlibstatic libcurl -static ${SOKULIB_LIBRARIES} SokuLib shady-core)
set_target_properties(shady-loader PROPERTIES LINK_FLAGS "/DELAYLOAD:SokuEngine.dll")
#set_target_properties(shady-loader PROPERTIES PREFIX "")

add_executable(shady-manage manager.cpp asynctask.cpp modpackage.cpp)
target_link_libraries(shady-manage zlibstatic libcurl -static)

install(TARGETS shady-loader LIBRARY DESTINATION soku-modules)

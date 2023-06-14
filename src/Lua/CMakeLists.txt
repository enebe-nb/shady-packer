set(SRC_LUA_Library
    logger.cpp
    script.cpp
    lualibs/base.cpp
    lualibs/gui.cpp
    lualibs/loader.cpp
    lualibs/memory.cpp
    lualibs/resource.cpp
    lualibs/soku.cpp
)
source_group("Source Files\\Library" FILES ${SRC_LUA_Library})

set(SRC_LUA_Module
    exports.cpp
)
source_group("Source Files\\Module" FILES ${SRC_LUA_Module})

add_library(shady-lualib STATIC ${SRC_LUA_Library})
target_link_libraries(shady-lualib shady-core liblua LuaBridge SokuLib)

add_library(shady-lua MODULE ${SRC_LUA_Module})
target_link_libraries(shady-lua -static shady-lualib)
set_target_properties(shady-lua PROPERTIES PREFIX "")
add_dependencies(shady-lua lfs liblua-socket liblua-mime)

install(TARGETS shady-lua LIBRARY DESTINATION modules/shady-lua)

# Install Extra Libraries
set(LUASOCKET_SRCPATH ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/luasocket/src)
install(TARGETS lfs LIBRARY DESTINATION modules/shady-lua/libs)
install(TARGETS liblua-mime LIBRARY DESTINATION modules/shady-lua/libs/mime)
install(TARGETS liblua-socket LIBRARY DESTINATION modules/shady-lua/libs/socket)
install(FILES
    ${LUASOCKET_SRCPATH}/http.lua
    ${LUASOCKET_SRCPATH}/url.lua
    ${LUASOCKET_SRCPATH}/tp.lua
    ${LUASOCKET_SRCPATH}/ftp.lua
    ${LUASOCKET_SRCPATH}/headers.lua
    ${LUASOCKET_SRCPATH}/smtp.lua
    DESTINATION modules/shady-lua/libs/socket
)
install(FILES
    ${LUASOCKET_SRCPATH}/ltn12.lua
    ${LUASOCKET_SRCPATH}/socket.lua
    ${LUASOCKET_SRCPATH}/mime.lua
    DESTINATION modules/shady-lua/libs
)

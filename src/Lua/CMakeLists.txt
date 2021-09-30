set(SRC_LUA_Library
    logger.cpp
    script.cpp
    lualibs/base.cpp
#    lualibs/imgui.cpp
    lualibs/memory.cpp
    lualibs/resource.cpp
    lualibs/soku.cpp
    lualibs/soku-hooks.cpp
)
source_group("Source Files\\Library" FILES ${SRC_LUA_Library})

set(SRC_LUA_Module
	exports.cpp
)
source_group("Source Files\\Module" FILES ${SRC_LUA_Module})

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(WARNING Skipping shady-lua: Can only build on Windows 32bits)
else()
    add_library(shady-lualib STATIC ${SRC_LUA_Library})
    target_link_libraries(shady-lualib shady-core liblua LuaBridge SokuLib)

    add_library(shady-lua MODULE ${SRC_LUA_Module})
    target_link_libraries(shady-lua -static shady-lualib)
    set_target_properties(shady-lua PROPERTIES PREFIX "")

    install(TARGETS shady-lua LIBRARY DESTINATION soku-modules)
endif()
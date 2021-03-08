set(SRC_LUALIB
    logger.cpp
    script.cpp
    lualibs/base.cpp
    lualibs/imgui.cpp
    lualibs/memory.cpp
    lualibs/resource.cpp
    lualibs/soku.cpp
    lualibs/soku-hooks.cpp
)

set(SRC_LUADLL exports.cpp)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(WARNING Skipping shady-lua: Can only build on Windows 32bits)
else()
    add_library(shady-lua STATIC ${SRC_LUALIB})
    target_link_libraries(shady-lua -static shady-core liblua LuaBridge imgui)

    add_library(shady-luadll MODULE ${SRC_LUADLL})
    target_link_libraries(shady-luadll -static shady-lua SokuLib)
    set_target_properties(shady-luadll PROPERTIES PREFIX "")

    install(TARGETS shady-lua LIBRARY DESTINATION soku-modules)
endif()
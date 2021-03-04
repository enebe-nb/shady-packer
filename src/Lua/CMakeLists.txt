set(SRC_LUA
    #exports.cpp
    logger.cpp
    script.cpp
    lualibs/base.cpp
    #lualibs/imgui.cpp
    lualibs/memory.cpp
    lualibs/resource.cpp
    lualibs/soku.cpp
    #../Loader/reader.cpp
)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(WARNING Skipping shady-lua: Can only build on Windows 32bits)
else()
    add_library(shady-lua STATIC ${SRC_LUA})
    target_link_libraries(shady-lua -static shady-core liblua LuaBridge)
    set_target_properties(shady-lua PROPERTIES PREFIX "")
    install(TARGETS shady-lua LIBRARY DESTINATION soku-modules)
endif()
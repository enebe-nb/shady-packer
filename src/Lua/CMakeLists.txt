include_directories(
    ${LUA_INCLUDE_DIR}
    ${SOKULIB_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${LIBZIP_INCLUDE_DIR})

add_definitions(-DIMGUI_DISABLE_INCLUDE_IMCONFIG_H -DSHADY_EXPORTS)
add_library(shady-lua MODULE
    exports.cpp
    logger.cpp
    script.cpp
    lualibs/base.cpp
    lualibs/memory.cpp
    lualibs/resource.cpp
    lualibs/soku.cpp
)
target_link_libraries(shady-lua zipstatic zlibstatic shlwapi -static ${SOKULIB_LIBRARIES} liblua shady-core)
set_target_properties(shady-lua PROPERTIES PREFIX "")

install(TARGETS shady-loader LIBRARY DESTINATION shady-loader)

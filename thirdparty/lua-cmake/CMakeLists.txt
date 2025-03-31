
project ( lua C )
cmake_minimum_required ( VERSION 3.16 )

set(LUA_PATH ../lua CACHE PATH "Path to lua library")

# Sources
set (SRC_CORE
  lzio.c
  lctype.c
  lopcodes.c
  lmem.c
  lundump.c
  ldump.c
  lstate.c
  lgc.c
  llex.c
  lcode.c
  lparser.c
  ldebug.c
  lfunc.c
  lobject.c
  ltm.c
  lstring.c
  ltable.c
  ldo.c
  lvm.c
  lapi.c
)
list(TRANSFORM SRC_CORE PREPEND ${LUA_PATH}/)
source_group ("Source Files\\Core" FILES ${SRC_ImGui})

set (SRC_LIB
  lauxlib.c
  lbaselib.c
  lcorolib.c
  ldblib.c
  liolib.c
  lmathlib.c
  loadlib.c
  loslib.c
  lstrlib.c
  ltablib.c
  lutf8lib.c
  linit.c
)
list(TRANSFORM SRC_LIB PREPEND ${LUA_PATH}/)
source_group ("Source Files\\Library" FILES ${SRC_ImGui})

## BUILD
add_library (liblua ${SRC_CORE} ${SRC_LIB})
target_include_directories (liblua PUBLIC . ${LUA_PATH})
set_target_properties(liblua PROPERTIES OUTPUT_NAME "lua")

add_executable (lua ${LUA_PATH}/lua.c)
target_link_libraries(lua PUBLIC liblua)

# Extra Libraries
set(LUAFILESYSTEM_PATH ../luafilesystem CACHE PATH "Path to lua-socket library")
add_library(lfs MODULE ${LUAFILESYSTEM_PATH}/src/lfs.c)
target_link_libraries(lfs -static liblua)
#set_target_properties(lfs PROPERTIES PREFIX "")

set(LUASOCKET_PATH ../luasocket CACHE PATH "Path to lua-socket library")
set(SRC_LIBLUA_Socket
  src/luasocket.c
  src/timeout.c
  src/buffer.c
  src/io.c
  src/auxiliar.c
  src/options.c
  src/inet.c
  src/except.c
  src/select.c
  src/tcp.c
  src/udp.c
  src/compat.c
)

if (WIN32)
  list(APPEND SRC_LIBLUA_Socket src/wsocket.c)
else()
  message(FATAL_ERROR "TODO Unix build")
endif()

set(SRC_LIBLUA_Mime src/mime.c src/compat.c)

list(TRANSFORM SRC_LIBLUA_Socket PREPEND ${LUASOCKET_PATH}/)
add_library(liblua-socket MODULE ${SRC_LIBLUA_Socket})
target_link_libraries(liblua-socket -static liblua ws2_32)
set_target_properties(liblua-socket PROPERTIES LIBRARY_OUTPUT_DIRECTORY "socket/" LIBRARY_OUTPUT_NAME "core")

list(TRANSFORM SRC_LIBLUA_Mime PREPEND ${LUASOCKET_PATH}/)
add_library(liblua-mime MODULE ${SRC_LIBLUA_Mime})
target_link_libraries(liblua-mime -static liblua)
set_target_properties(liblua-mime PROPERTIES LIBRARY_OUTPUT_DIRECTORY "mime/" LIBRARY_OUTPUT_NAME "core")

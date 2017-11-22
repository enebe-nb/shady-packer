include_directories(${LIBZIP_INCLUDE_DIR})

add_library(shady-loader MODULE loader.cpp)
target_link_libraries(shady-loader zipstatic zlibstatic shlwapi -static)
set_target_properties(shady-loader PROPERTIES PREFIX "")
install(TARGETS shady-loader LIBRARY DESTINATION loader)

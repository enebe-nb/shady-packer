include_directories(
    ${RTAUDIO_INCLUDE_DIR}
    ${GTK3_INCLUDE_DIRS}
)

set(SRC_GUI_Root
	application.hpp
	application.cpp
	renderer.hpp
	renderer.cpp
	dialog.hpp
	dialog.cpp
	editor.hpp
	editor.cpp
)

if(WIN32)
	set(SHADY_VERSION 1.0.0.0)
	string(REPLACE . , SHADY_VERSION_COMMA ${SHADY_VERSION})
	set(SHADY_TARGET_NAME "shady-viewer")
	set(SHADY_NAME "Shady Viewer")
	set(SHADY_DESCRIPTION "A specific fighting game resource viewer.")
	set(SHADY_ICON "viewer.ico")
	configure_file(resource.rc.in resource.rc @ONLY)
	file(COPY ${SHADY_ICON} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND SRC_GUI_Root ${CMAKE_CURRENT_BINARY_DIR}/resource.rc)
endif()

source_group("Source Files" FILES ${SRC_GUI_Root})

add_executable(shady-viewer WIN32 ${SRC_GUI_Root})
#link_directories(${GTK3_LIBRARY_DIRS})
#add_definitions(${GTK3_CFLAGS_OTHER})
target_link_libraries(shady-viewer shady-core rtaudio ${GTK3_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

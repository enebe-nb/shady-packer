include_directories(
    ${RTAUDIO_INCLUDE_DIR}
    ${GTK3_INCLUDE_DIRS}
)

set(SRC_GUI_Root
	application.hpp
	application.cpp
	renderer.hpp
	renderer.cpp

	#window.hpp
	#window.cpp
	dialog.hpp
	dialog.cpp
	editor.hpp
	editor.cpp
)
source_group("Source Files" FILES ${SRC_GUI_Root})

add_executable(shady-viewer WIN32 ${SRC_GUI_Root})
#link_directories(${GTK3_LIBRARY_DIRS})
#add_definitions(${GTK3_CFLAGS_OTHER})
target_link_libraries(shady-viewer shady-core rtaudio ${GTK3_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

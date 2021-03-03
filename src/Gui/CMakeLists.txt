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
	${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)
source_group("Source Files" FILES ${SRC_GUI_Root})

if(WIN32)
	set(SHADY_TARGET_NAME "shady-viewer")
	set(SHADY_NAME "Shady Viewer")
	set(SHADY_DESCRIPTION "A specific fighting game resource viewer.")
	configure_file(../resource.rc.in resource.rc @ONLY)
	file(COPY ../icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_executable(shady-viewer WIN32 ${SRC_GUI_Root})
target_link_libraries(shady-viewer shady-core rtaudio ${GTK3_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS shady-viewer RUNTIME DESTINATION bin)
if (WIN32)
	install(FILES $<TARGET_FILE:rtaudio> DESTINATION bin)
	
	get_filename_component(SYSTEM_BINARY_DIR ${PKG_CONFIG_EXECUTABLE} DIRECTORY)
	foreach(LIBRARY ${GTK3_STATIC_LIBRARIES})
		FILE(GLOB DLL_LIBRARY "${SYSTEM_BINARY_DIR}/lib${LIBRARY}*.dll")
		list(APPEND DLL_LIBRARIES ${DLL_LIBRARY})
	endforeach()
	list(APPEND DLL_LIBRARIES ${SYSTEM_BINARY_DIR}/zlib1.dll)
	install(FILES ${DLL_LIBRARIES} DESTINATION bin)
	
	if(NOT MSVC)
		foreach(LIBRARY gcc_s stdc++ winpthread)
			FILE(GLOB RUNTIME_LIBRARY "${SYSTEM_BINARY_DIR}/lib${LIBRARY}*.dll" )
			list(APPEND RUNTIME_LIBRARIES ${RUNTIME_LIBRARY})
		endforeach()	
		install(FILES ${RUNTIME_LIBRARIES} DESTINATION bin)
	endif()
	
	get_filename_component(SYSTEM_DIR ${SYSTEM_BINARY_DIR} DIRECTORY)
	install(DIRECTORY ${SYSTEM_DIR}/share/icons/Adwaita DESTINATION share/icons)
	install(DIRECTORY ${SYSTEM_DIR}/share/glib-2.0 DESTINATION share)
	install(DIRECTORY ${SYSTEM_DIR}/lib/gdk-pixbuf-2.0 DESTINATION lib)
else()
	install(FILES $<TARGET_FILE:rtaudio> DESTINATION lib)
endif()

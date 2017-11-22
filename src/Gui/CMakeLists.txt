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
else()
	install(FILES $<TARGET_FILE:rtaudio> DESTINATION lib)
endif()

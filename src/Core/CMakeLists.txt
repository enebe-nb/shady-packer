link_directories(${BOOST_LIBRARY_DIR})
include_directories(
    ${RAPIDXML_INCLUDE_DIR}
    ${LIBZIP_INCLUDE_DIR}
    ${LIBPNG_INCLUDE_DIR}
    ${BOOST_INCLUDE_DIR}
)

set(SRC_CORE_Resource
	resource/image.cpp
	resource/gui.cpp
	resource/pattern.cpp
	resource/sfx.cpp
	resource/text.cpp
	resource/readerwriter.hpp
	resource/readerwriter.cpp )
source_group("Source Files\\resource" FILES ${SRC_CORE_Resource})

set(SRC_CORE_Util
	util/iohelper.hpp
    util/riffdocument.hpp
    util/singleton.hpp
    util/strallocator.hpp
	util/xmlprinter.hpp
    util/util.cpp )
source_group("Source Files\\util" FILES ${SRC_CORE_Util})

set(SRC_CORE_Root
	baseentry.hpp
	dataentry.hpp
	dataentry.cpp
	fileentry.hpp
	fileentry.cpp
	zipentry.hpp
	zipentry.cpp

	resource.hpp
	package.hpp
	package.cpp
	packagefilter.cpp)
source_group("Source Files" FILES ${SRC_CORE_Root})

add_library(shady-core STATIC ${SRC_CORE_Resource} ${SRC_CORE_Util} ${SRC_CORE_Root})
target_link_libraries(shady-core zipstatic png_static ${BOOST_LIBRARIES})
target_compile_definitions(shady-core PRIVATE USEWINDOWS_CALLBACK)
add_dependencies(shady-core boost)
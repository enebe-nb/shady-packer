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
	reader.hpp
	reader.cpp
	package.hpp
	package.cpp
	packageslash.cpp
)
source_group("Source Files" FILES ${SRC_CORE_Root})

add_library(shady-core STATIC ${SRC_CORE_Resource} ${SRC_CORE_Util} ${SRC_CORE_Root})
target_link_libraries(shady-core zip png_static rapidxml)
target_compile_definitions(shady-core PRIVATE USEWINDOWS_CALLBACK)
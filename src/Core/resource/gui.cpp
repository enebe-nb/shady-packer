#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/xmlprinter.hpp"
#include <rapidxml.hpp>
#include <vector>
#include <cstring>

ShadyCore::GuiRoot::~GuiRoot() {
	for (auto i : images) delete i;
	for (auto s : views) delete s;
}

ShadyCore::GuiImage::~GuiImage() {
    if (name) delete[] name;
}

void ShadyCore::GuiImage::initialize(const char* n) {
	size_t length = strlen(n);

	if (name) delete[] name;
	name = new char[length + 1];
	strcpy((char*)name, n);
}

ShadyCore::GuiView::~GuiView() {
    if (impl) delete impl;
}

void ShadyCore::GuiView::initialize(uint32_t i, Type t) {
    id = i; type = t;

    if (impl) delete impl;
    switch(type) {
    case VIEW_STATIC: impl = new Static(*this); break;
    case VIEW_MUTABLE: impl = new Mutable(*this); break;
    case VIEW_SLIDERHORZ: impl = new SliderHorz(*this); break;
    case VIEW_SLIDERVERT: impl = new SliderVert(*this); break;
    case VIEW_NUMBER: impl = new Number(*this); break;
    }
}

void ShadyCore::GuiRoot::initialize() {
	for (auto i : images) delete i;
	for (auto s : views) delete s;
	images.clear();
	views.clear();
}

static const char* itoa(int input) {
	static char output[12];
	sprintf(output, "%d", input);
	return output;
}

static const char * typeNames[] = {
	"static",
	"mutable",
	"sliderhorz",
	"slidervert",
	"unused04",
	"unused05",
	"number",
};

namespace {
	class XmlReaderData : public ShadyCore::ResourceVisitor::TempData {
	private:
		rapidxml::xml_document<> document;
		char* data;
	public:
		rapidxml::xml_node<>* current;

		inline XmlReaderData(std::istream& input) {
			input.seekg(0, std::ios::end);
			size_t len = input.tellg();
			input.seekg(0);

			data = new char[len + 1];
			data[len] = '\0';
			input.read(data, len);

			document.parse<0>(data);
			current = document.first_node();
		}

		inline ~XmlReaderData() { delete[] data; }
	};

	class XmlWriterData : public ShadyCore::ResourceVisitor::TempData {
	public:
		ShadyUtil::XmlPrinter printer;
		inline XmlWriterData(std::ostream& output) : printer(output) {}
	};
}

static ShadyCore::GuiImage* findImage(ShadyCore::GuiRoot* root, const char* name) {
	uint32_t i, count = root->getImageCount();
	for (i = 0; i < count && strcmp(root->getImage(i).getName(), name) != 0; ++i);
	if (i < count) return &root->getImage(i);
	return 0;
}

void ShadyCore::ResourceDReader::accept(GuiImage& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

	resource.initialize(readerData->current->first_attribute("name")->value());
    ShadyUtil::readX(readerData->current, "xposition", resource, &GuiImage::setX);
    ShadyUtil::readX(readerData->current, "yposition", resource, &GuiImage::setY);
    ShadyUtil::readX(readerData->current, "width", resource, &GuiImage::setWidth);
    ShadyUtil::readX(readerData->current, "height", resource, &GuiImage::setHeight);
}

void ShadyCore::ResourceEReader::accept(GuiImage& resource) {
	uint32_t count; char buffer[128];
	input.read((char*)&count, 4);
	input.read(buffer, count);
	buffer[count] = '\0';
	resource.initialize(buffer);

	ShadyUtil::readS(input, resource, &GuiImage::setX);
	ShadyUtil::readS(input, resource, &GuiImage::setY);
	ShadyUtil::readS(input, resource, &GuiImage::setWidth);
	ShadyUtil::readS(input, resource, &GuiImage::setHeight);
}


void ShadyCore::ResourceDReader::accept(GuiView& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

	int type = 0; while (strcmp(readerData->current->name(), typeNames[type]) != 0) ++type;
	resource.initialize(atoi(readerData->current->first_attribute("id")->value()), (ShadyCore::GuiView::Type)type);
}

void ShadyCore::ResourceEReader::accept(GuiView& resource) {
	uint32_t id; uint8_t type;
	input.read((char*)&id, 4);
	input.read((char*)&type, 1);
	resource.initialize(id, (ShadyCore::GuiView::Type)type);
}

void ShadyCore::ResourceDReader::accept(GuiView::Static& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

    ShadyUtil::readX(readerData->current, "xposition", (GuiView::Impl&)resource, &GuiView::Impl::setX);
    ShadyUtil::readX(readerData->current, "yposition", (GuiView::Impl&)resource, &GuiView::Impl::setY);
    ShadyUtil::readX(readerData->current, "mirror", resource, &GuiView::Static::setMirror);

	GuiImage* image = findImage(resource.getRoot(), readerData->current->first_node("image")->first_attribute("name")->value());
	if (!image) {
		image = &resource.getRoot()->createImage();
		readerData->current = readerData->current->first_node("image");
		image->visit(this);
		readerData->current = readerData->current->parent();
	} resource.setImage(image);
}

void ShadyCore::ResourceEReader::accept(GuiView::Static& resource) {
	int32_t index; input.read((char*)&index, 4);
	resource.setImage(&resource.getRoot()->getImage(index));
	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setX);
	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setY);
	ShadyUtil::readS(input, resource, &GuiView::Static::setMirror);
}

void ShadyCore::ResourceDReader::accept(GuiView::Mutable& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

    ShadyUtil::readX(readerData->current, "xposition", (GuiView::Impl&)resource, &GuiView::Impl::setX);
    ShadyUtil::readX(readerData->current, "yposition", (GuiView::Impl&)resource, &GuiView::Impl::setY);
}

void ShadyCore::ResourceEReader::accept(GuiView::Mutable& resource) {
	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setX);
	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setY);
}

void ShadyCore::ResourceDReader::accept(GuiView::Number& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

	ShadyUtil::readX(readerData->current, "xposition", (GuiView::Impl&)resource, &GuiView::Impl::setX);
    ShadyUtil::readX(readerData->current, "yposition", (GuiView::Impl&)resource, &GuiView::Impl::setY);
    ShadyUtil::readX(readerData->current, "width", resource, &GuiView::Number::setWidth);
    ShadyUtil::readX(readerData->current, "height", resource, &GuiView::Number::setHeight);
    ShadyUtil::readX(readerData->current, "fontspacing", resource, &GuiView::Number::setFontSpacing);
    ShadyUtil::readX(readerData->current, "textspacing", resource, &GuiView::Number::setTextSpacing);
    ShadyUtil::readX(readerData->current, "size", resource, &GuiView::Number::setSize);
    ShadyUtil::readX(readerData->current, "floatsize", resource, &GuiView::Number::setFloatSize);

	GuiImage* image = findImage(resource.getRoot(), readerData->current->first_node("image")->first_attribute("name")->value());
	if (!image) {
		image = &resource.getRoot()->createImage();
		readerData->current = readerData->current->first_node("image");
		image->visit(this);
		readerData->current = readerData->current->parent();
	} resource.setImage(image);
}

void ShadyCore::ResourceEReader::accept(GuiView::Number& resource) {
	int32_t index; input.read((char*)&index, 4);
	resource.setImage(&resource.getRoot()->getImage(index));

	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setX);
	ShadyUtil::readS(input, (GuiView::Impl&)resource, &GuiView::Impl::setY);
	ShadyUtil::readS(input, resource, &GuiView::Number::setWidth);
	ShadyUtil::readS(input, resource, &GuiView::Number::setHeight);
	ShadyUtil::readS(input, resource, &GuiView::Number::setFontSpacing);
	ShadyUtil::readS(input, resource, &GuiView::Number::setTextSpacing);
	ShadyUtil::readS(input, resource, &GuiView::Number::setSize);
	ShadyUtil::readS(input, resource, &GuiView::Number::setFloatSize);
}

void ShadyCore::ResourceDReader::accept(GuiRoot& resource) {
	if (!tempData) tempData = new XmlReaderData(input);
	XmlReaderData* readerData = (XmlReaderData*) tempData;

	resource.initialize();
	for (rapidxml::xml_node<>* iter = readerData->current = readerData->current->first_node(); iter; iter = readerData->current = iter->next_sibling()) {
        resource.createView().visit(this);
	}
}

void ShadyCore::ResourceEReader::accept(GuiRoot& resource) {
	uint32_t count;
	input.ignore(4);
	input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.createImage().visit(this);
	}

	input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.createView().visit(this);
	}
}

void ShadyCore::ResourceDWriter::accept(GuiImage& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*) tempData;
	
	writerData->printer.openNode("image");
	writerData->printer.appendAttribute("name", resource.getName());
	writerData->printer.appendAttribute("xposition", resource.getX());
	writerData->printer.appendAttribute("yposition", resource.getY());
	writerData->printer.appendAttribute("width", resource.getWidth());
	writerData->printer.appendAttribute("height", resource.getHeight());
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(GuiImage& resource) {
	uint32_t count = strlen(resource.getName());
	output.write((char*)&count, 4);
	output.write(resource.getName(), count);

	ShadyUtil::writeS(output, resource.getX());
	ShadyUtil::writeS(output, resource.getY());
	ShadyUtil::writeS(output, resource.getWidth());
	ShadyUtil::writeS(output, resource.getHeight());
}

void ShadyCore::ResourceDWriter::accept(GuiView& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode(typeNames[resource.getType()]);
	writerData->printer.appendAttribute("id", resource.getId());
}

void ShadyCore::ResourceEWriter::accept(GuiView& resource) {
	ShadyUtil::writeS(output, resource.getId());
	uint8_t type = (uint8_t)resource.getType();
	ShadyUtil::writeS(output, type);

	//if (resource.getType() != ShadyCore::GuiView::VIEW_MUTABLE) {
	// TODO reselect images
	//uint32_t index = 0; while (&resource.getRoot()->getImage(index) != resource.getImage()) ++index;
	//output.write((char*)&index, 4);
	//}
}

void ShadyCore::ResourceDWriter::accept(GuiView::Static& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("xposition", resource.getX());
    writerData->printer.appendAttribute("yposition", resource.getY());
	writerData->printer.appendAttribute("mirror", resource.getMirror());
	((ShadyCore::GuiImage*)resource.getImage())->visit(this);
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(GuiView::Static& resource) {
	uint32_t index = 0; while (&resource.getRoot()->getImage(index) != resource.getImage()) ++index;
	ShadyUtil::writeS(output, index);
	ShadyUtil::writeS(output, resource.getX());
	ShadyUtil::writeS(output, resource.getY());
	ShadyUtil::writeS(output, resource.getMirror());
}

void ShadyCore::ResourceDWriter::accept(GuiView::Mutable& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("xposition", resource.getX());
    writerData->printer.appendAttribute("yposition", resource.getY());
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(GuiView::Mutable& resource) {
	ShadyUtil::writeS(output, resource.getX());
	ShadyUtil::writeS(output, resource.getY());
}

void ShadyCore::ResourceDWriter::accept(GuiView::Number& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.appendAttribute("xposition", resource.getX());
    writerData->printer.appendAttribute("yposition", resource.getY());
	writerData->printer.appendAttribute("width", resource.getWidth());
	writerData->printer.appendAttribute("height", resource.getHeight());
	writerData->printer.appendAttribute("fontspacing", resource.getFontSpacing());
	writerData->printer.appendAttribute("textspacing", resource.getTextSpacing());
	writerData->printer.appendAttribute("size", resource.getSize());
	writerData->printer.appendAttribute("floatsize", resource.getFloatSize());
	((ShadyCore::GuiImage*)resource.getImage())->visit(this);
	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(GuiView::Number& resource) {
	uint32_t index = 0; while (&resource.getRoot()->getImage(index) != resource.getImage()) ++index;
	ShadyUtil::writeS(output, index);
	ShadyUtil::writeS(output, resource.getX());
	ShadyUtil::writeS(output, resource.getY());
	ShadyUtil::writeS(output, resource.getWidth());
	ShadyUtil::writeS(output, resource.getHeight());
	ShadyUtil::writeS(output, resource.getFontSpacing());
	ShadyUtil::writeS(output, resource.getTextSpacing());
	ShadyUtil::writeS(output, resource.getSize());
	ShadyUtil::writeS(output, resource.getFloatSize());
}

void ShadyCore::ResourceDWriter::accept(GuiRoot& resource) {
	if (!tempData) tempData = new XmlWriterData(output);
	XmlWriterData* writerData = (XmlWriterData*)tempData;

	writerData->printer.openNode("layout");
	writerData->printer.appendAttribute("version", "4");

	for (uint32_t i = 0; i < resource.getViewCount(); ++i) {
		resource.getView(i).visit(this);
	}

	writerData->printer.closeNode();
}

void ShadyCore::ResourceEWriter::accept(GuiRoot& resource) {
	uint32_t count = resource.getImageCount();
	output.write("\x04\0\0\0", 4);
	output.write((char*)&count, 4);
	// TODO reselect images
	for (uint32_t i = 0; i < count; ++i) {
		resource.getImage(i).visit(this);
	}

	count = resource.getViewCount();
	output.write((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		resource.getView(i).visit(this);
	}
}
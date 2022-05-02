#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/xmlprinter.hpp"
#include <vector>
#include <cstring>

namespace _private {
void readerSchemaGui(ShadyCore::Schema& resource, std::istream& input) {
	uint32_t count;
	input.read((char*)&count, 4);
	if (count != 4) return; // version is not "4"

	input.read((char*)&count, 4);
	resource.images.reserve(count);
	for (int i = 0; i < count; ++i) {
		uint32_t len; input.read((char*)&len, 4);
		char* name = new char[len + 1];
		input.read(name, len); name[len] = '\0';
		auto& image = resource.images.emplace_back(name);
		input.read((char*)&image.x, 16); // , y, w, h
	};

	input.read((char*)&count, 4);
	resource.objects.reserve(count);
	for (int i = 0; i < count; ++i) {
		uint32_t id; uint8_t type;
		input.read((char*)&id, 4);
		input.read((char*)&type, 1);
		ShadyCore::Schema::GuiObject* object;
		switch (type) {
		case 0: case 2: case 3: case 4: case 5:
			object = new ShadyCore::Schema::GuiObject(id, type);
			input.read((char*)&object->imageIndex, 16); // , x, y, mirror
			break;
		case 1:
			object = new ShadyCore::Schema::GuiObject(id, type);
			input.read((char*)&object->x, 8); // , y
			break;
		case 6:
			object = new ShadyCore::Schema::GuiNumber(id);
			input.read((char*)&object->imageIndex, 12); // , x, y
			input.read((char*)&((ShadyCore::Schema::GuiNumber*)object)->w, 24);
				// , h, fontSpacing, textSpacing, size, floatSize
			break;
		default:
			throw std::runtime_error("Schema Gui: Unrecognized type of object.");
		}
		resource.objects.push_back(object);
	}
}

void writerSchemaGui(ShadyCore::Schema& resource, std::ostream& output) {
	output.write("\x04\0\0\0", 4);
	uint32_t count = resource.images.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto& image = resource.images[i];
		uint32_t len = strlen(image.name);
		output.write((char*)&len, 4);
		output.write(image.name, len);
		output.write((char*)&image.x, 16);
	}

	count = resource.objects.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		ShadyCore::Schema::GuiObject* object = (ShadyCore::Schema::GuiObject*)resource.objects[i];
		output.write((char*)&object->getId(), 4);
		output.write((char*)&object->getType(), 1);
		switch (object->getType()) {
		case 0: case 2: case 3: case 4: case 5:
			output.write((char*)&object->imageIndex, 16); // , x, y, mirror
			break;
		case 1:
			output.write((char*)&object->x, 8); // , y
			break;
		case 6:
			output.write((char*)&object->imageIndex, 12); // , x, y
			output.write((char*)&((ShadyCore::Schema::GuiNumber*)object)->w, 24);
				// , h, fontSpacing, textSpacing, size, floatSize
			break;
		}
	}
}
}
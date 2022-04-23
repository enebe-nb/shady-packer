#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include <cstring>

static void readerSchemaGame(ShadyCore::Schema& resource, std::istream& input, ShadyCore::Schema::Object* (*fnReadObject)(uint32_t, uint32_t, std::istream&)) {
	uint32_t count = 0;
	input.read((char*)&count, 1);
	if (count != 5) return; // version is not "5"

	char buffer[129];
	input.read((char*)&count, 2);
	resource.images.reserve(count);
	for (int i = 0; i < count; ++i) {
		input.read(buffer, 128);
		uint32_t len = strnlen(buffer, 128);
		buffer[len] = '\0';
		char* name = new char[len + 1];
		strcpy(name, buffer);
		resource.images.emplace_back(name);
	}

	uint32_t lastId = -1, lastIndex = -1, curFrameIndex = -1;
	input.read((char*)&count, 4);
	resource.objects.reserve(count);
	input.peek(); while (!input.eof()) {
		int32_t id; input.read((char*)&id, 4);
		if (id == -1) {
			input.read((char*)&id, 4);
			auto object = new ShadyCore::Schema::Clone(id);
			input.read((char*)&id, 4);
			object->setTargetId(id);
			resource.objects.push_back(object);
		} else if (id == -2) {
			resource.objects.push_back(fnReadObject(lastId, ++lastIndex, input));
		} else {
			auto i = resource.objects.begin(), end = resource.objects.end(); for(; i != end; ++i) {
				if ((*i)->getId() == id) break;
			} while (i != end && (*i)->getId() == id) i = resource.objects.erase(i);
			resource.objects.push_back(fnReadObject(lastId = id, lastIndex = 0, input));
		}
		input.peek();
	}
}

static ShadyCore::Schema::Object* readerSchemaAnimObject(uint32_t id, uint32_t index, std::istream& input) {
	auto object = new ShadyCore::Schema::Sequence(id, true);
	input.read((char*)&object->hasLoop(), 1);

	uint32_t count; input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		auto frame = new ShadyCore::Schema::Sequence::Frame();
		object->frames.push_back(frame);

		input.read(frame->getDataAddr(), 19);
		if (frame->hasBlendOptions()) {
			// TODO packed load
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setMode);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setColor);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setScaleX);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setScaleY);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setFlipVert);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setFlipHorz);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setAngle);
		}
	}

	return object;
}

static ShadyCore::Schema::Object* readerSchemaMoveObject(uint32_t id, uint32_t index, std::istream& input) {
	auto object = new ShadyCore::Schema::Sequence(id, false);
	input.read(object->getDataAddr(), 5);

	uint32_t count; input.read((char*)&count, 4);
	for (uint32_t i = 0; i < count; ++i) {
		auto frame = new ShadyCore::Schema::Sequence::MoveFrame();
		object->frames.push_back(frame);

		input.read(frame->getDataAddr(), 19);
		if (frame->hasBlendOptions()) {
			// TODO packed load
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setMode);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setColor);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setScaleX);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setScaleY);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setFlipVert);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setFlipHorz);
			ShadyUtil::readS(input, frame->getBlendOptions(), &ShadyCore::Schema::Sequence::BlendOptions::setAngle);
		}

		input.read(frame->getTraits().getDataAddr(), 40);
		ShadyUtil::readS(input, frame->getTraits(), &ShadyCore::Schema::Sequence::MoveTraits::setComboModifier);
		ShadyUtil::readS(input, frame->getTraits(), &ShadyCore::Schema::Sequence::MoveTraits::setFrameFlags);
		ShadyUtil::readS(input, frame->getTraits(), &ShadyCore::Schema::Sequence::MoveTraits::setAttackFlags);

		uint8_t boxes;
		input.read((char*)&boxes, 1);
		for (uint8_t j = 0; j < boxes; ++j) {
			auto& box = frame->getCollisionBoxes().createBox();
			input.read(box.getDataAddr(), 16);
		}

		input.read((char*)&boxes, 1);
		for (uint8_t j = 0; j < boxes; ++j) {
			auto& box = frame->getHitBoxes().createBox();
			input.read(box.getDataAddr(), 16);
		}

		input.read((char*)&boxes, 1);
		for (uint8_t j = 0; j < boxes; ++j) {
			auto& box = frame->getAttackBoxes().createBox();
			input.read(box.getDataAddr(), 17);
		}

		input.read(frame->getEffect().getDataAddr(), 30);
	}

	return object;
}

namespace _private {
void readerSchemaAnim(ShadyCore::Schema& resource, std::istream& input) {
	readerSchemaGame(resource, input, readerSchemaAnimObject);
}

void readerSchemaMove(ShadyCore::Schema& resource, std::istream& input) {
	readerSchemaGame(resource, input, readerSchemaMoveObject);
}
}

static void writerSchemaGame(ShadyCore::Schema& resource, std::ostream& output, void (*fnWriteObject)(ShadyCore::Schema::Sequence*, std::ostream&)) {
	output.put('\x05');
	uint32_t count = resource.images.size();
	output.write((char*)&count, 2);
	for (int i = 0; i < count; ++i) {
		size_t len = strlen(resource.images[i].getName());
		output.write(resource.images[i].getName(), len + 1);
		while (++len < 128) output.put(0);
	}

	// TODO Sort sequences and frames
	uint32_t lastId = -1;
	count = resource.objects.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto object = resource.objects[i];
		if (object->getType() == 7) {
			output.write("\xff\xff\xff\xff", 4);
			output.write((char*)&((ShadyCore::Schema::Clone*)object)->getId(), 4);
			output.write((char*)&((ShadyCore::Schema::Clone*)object)->getTargetId(), 4);
		} else {
			if (lastId == object->getId()) output.write("\xff\xff\xff\xfe", 4);
			else output.write((char*) &object->getId(), 4);
			lastId = object->getId();
			fnWriteObject((ShadyCore::Schema::Sequence*)object, output);
		}
	}
}

static void writerSchemaAnimObject(ShadyCore::Schema::Sequence* resource, std::ostream& output) {
	output.write((char*)&resource->hasLoop(), 1);
	uint32_t count = resource->frames.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto frame = resource->frames[i];
		output.write(frame->getDataAddr(), 19);
		if (frame->hasBlendOptions()) {
			// TODO packed load
			ShadyUtil::writeS(output, frame->getBlendOptions().getMode());
			ShadyUtil::writeS(output, frame->getBlendOptions().getColor());
			ShadyUtil::writeS(output, frame->getBlendOptions().getScaleX());
			ShadyUtil::writeS(output, frame->getBlendOptions().getScaleY());
			ShadyUtil::writeS(output, frame->getBlendOptions().getFlipVert());
			ShadyUtil::writeS(output, frame->getBlendOptions().getFlipHorz());
			ShadyUtil::writeS(output, frame->getBlendOptions().getAngle());
		}
	}
}

static void writerSchemaMoveObject(ShadyCore::Schema::Sequence* resource, std::ostream& output) {
	output.write(resource->getDataAddr(), 5);
	uint32_t count = resource->frames.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto frame = (ShadyCore::Schema::Sequence::MoveFrame*)resource->frames[i];
		output.write(frame->getDataAddr(), 19);
		if (frame->hasBlendOptions()) {
			// TODO packed load
			ShadyUtil::writeS(output, frame->getBlendOptions().getMode());
			ShadyUtil::writeS(output, frame->getBlendOptions().getColor());
			ShadyUtil::writeS(output, frame->getBlendOptions().getScaleX());
			ShadyUtil::writeS(output, frame->getBlendOptions().getScaleY());
			ShadyUtil::writeS(output, frame->getBlendOptions().getFlipVert());
			ShadyUtil::writeS(output, frame->getBlendOptions().getFlipHorz());
			ShadyUtil::writeS(output, frame->getBlendOptions().getAngle());
		}

		output.write(frame->getTraits().getDataAddr(), 40);
		ShadyUtil::writeS(output, frame->getTraits().getComboModifier());
		ShadyUtil::writeS(output, frame->getTraits().getFrameFlags());
		ShadyUtil::writeS(output, frame->getTraits().getAttackFlags());

		uint8_t boxes = frame->getCollisionBoxes().getBoxCount();
		output.write((char*)&boxes, 1);
		for (int i = 0; i < boxes; ++i) {
			output.write(frame->getCollisionBoxes().getBox(i).getDataAddr(), 16);
		}

		boxes = frame->getHitBoxes().getBoxCount();
		output.write((char*)&boxes, 1);
		for (int i = 0; i < boxes; ++i) {
			output.write(frame->getHitBoxes().getBox(i).getDataAddr(), 16);
		}

		boxes = frame->getAttackBoxes().getBoxCount();
		output.write((char*)&boxes, 1);
		for (int i = 0; i < boxes; ++i) {
			output.write(frame->getAttackBoxes().getBox(i).getDataAddr(), 17);
		}

		output.write(frame->getEffect().getDataAddr(), 30);
	}
}

namespace _private {
void writerSchemaAnim(ShadyCore::Schema& resource, std::ostream& output) {
	writerSchemaGame(resource, output, writerSchemaAnimObject);
}

void writerSchemaMove(ShadyCore::Schema& resource, std::ostream& output) {
	writerSchemaGame(resource, output, writerSchemaMoveObject);
}
}
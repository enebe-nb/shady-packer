#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include <cstring>

ShadyCore::Schema::Image::~Image() { delete[] name; }
ShadyCore::Schema::Sequence::~Sequence() { for (auto frame : frames) delete frame; }

static void readerSchemaGame(ShadyCore::Schema& resource, std::istream& input, ShadyCore::Schema::Object* (*fnReadObject)(uint32_t, uint32_t, std::istream&)) {
	uint32_t count = 0;
	input.read((char*)&count, 1);
	if (count != 5) return; // version is not "5"

	input.read((char*)&count, 2);
	resource.images.reserve(count);
	for (int i = 0; i < count; ++i) {
		char* name = new char[129];
		input.read(name, 128); name[128] = '\0';
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
			input.read((char*)&object->targetId, 4);
			resource.objects.push_back(object);
		} else if (id == -2) {
			resource.objects.push_back(fnReadObject(lastId, ++lastIndex, input));
		} else {
			auto i = resource.objects.begin(), end = resource.objects.end(); for(; i != end; ++i) {
				if ((*i)->getId() == id) break;
			} while (i != resource.objects.end() && (*i)->getId() == id) i = resource.objects.erase(i);
			resource.objects.push_back(fnReadObject(lastId = id, lastIndex = 0, input));
		}
		input.peek();
	}
}

static ShadyCore::Schema::Object* readerSchemaAnimObject(uint32_t id, uint32_t index, std::istream& input) {
	auto object = new ShadyCore::Schema::Sequence(id, true);
	input.read((char*)&object->loop, 1);

	uint32_t count; input.read((char*)&count, 4);
	object->frames.reserve(count);
	for (uint32_t i = 0; i < count; ++i) {
		auto frame = new ShadyCore::Schema::Sequence::Frame();
		object->frames.push_back(frame);

		input.read((char*)&frame->imageIndex, 19);
			// , unknown, texOffsetX/Y, texW/H, offsetX/Y, duration, renderGroup
		if (frame->hasBlendOptions()) {
			input.read((char*)&frame->blendOptions.mode, 2);
			input.read((char*)&frame->blendOptions.color, 14); // , scaleX/Y, flipV/H, angle
		}
	}

	return object;
}

static ShadyCore::Schema::Object* readerSchemaMoveObject(uint32_t id, uint32_t index, std::istream& input) {
	auto object = new ShadyCore::Schema::Sequence(id, false);
	input.read((char*)&object->moveLock, 5); // , actionLock, loop

	uint32_t count; input.read((char*)&count, 4);
	object->frames.reserve(count);
	for (uint32_t i = 0; i < count; ++i) {
		auto frame = new ShadyCore::Schema::Sequence::MoveFrame();
		object->frames.push_back(frame);

		input.read((char*)&frame->imageIndex, 19);
			// , unknown, texOffsetX/Y, texW/H, offsetX/Y, duration, renderGroup
		if (frame->hasBlendOptions()) {
			input.read((char*)&frame->blendOptions.mode, 2);
			input.read((char*)&frame->blendOptions.color, 14); // , scaleX/Y, flipV/H, angle
		}

		input.read((char*)&frame->traits.damage, 40);
			// , proration, chipDamage, spiritDamage, untech, power, limit
			// , onHitPlayerStun, onHitEnemyStun, onBlockPlayerStun, onBlockEnemyStun
			// , onHitCardGain, onBlockCardGain, onAirHitSetSequence, onGroundHitSetSequence
			// , speedX, speedY, onHitSfx, onHitEffect, attackLevel
		input.read((char*)&frame->traits.comboModifier, 1);
		input.read((char*)&frame->traits.frameFlags, 4);
		input.read((char*)&frame->traits.attackFlags, 4);

		uint8_t boxCount;
		input.read((char*)&boxCount, 1);
		frame->cBoxes.reserve(boxCount);
		for (uint8_t j = 0; j < boxCount; ++j) {
			auto& box = frame->cBoxes.emplace_back();
			input.read((char*)&box.left, 16); // , up, right, down
		}

		input.read((char*)&boxCount, 1);
		frame->hBoxes.reserve(boxCount);
		for (uint8_t j = 0; j < boxCount; ++j) {
			auto& box = frame->hBoxes.emplace_back();
			input.read((char*)&box.left, 16); // , up, right, down
		}

		input.read((char*)&boxCount, 1);
		frame->aBoxes.reserve(boxCount);
		for (uint8_t j = 0; j < boxCount; ++j) {
			auto& box = frame->aBoxes.emplace_back();
			input.read((char*)&box.left, 16); // , up, right, down
			char hasExtra; input.read(&hasExtra, 1);
			if (hasExtra) {
				box.extra = new ShadyCore::Schema::Sequence::BBox;
				input.read((char*)box.extra, 16); // , up, right, down
			}
		}

		input.read((char*)&frame->effect.pivotX, 30);
			// , pivotY,  positionX/YExtra, positionX/Y, unknown02RESETSTATE, speedX/Y
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
		size_t len = strlen(resource.images[i].name);
		output.write(resource.images[i].name, len + 1);
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
			output.write((char*)&((ShadyCore::Schema::Clone*)object)->targetId, 4);
		} else {
			if (lastId == object->getId()) output.write("\xfe\xff\xff\xff", 4);
			else output.write((char*) &object->getId(), 4);
			lastId = object->getId();
			fnWriteObject((ShadyCore::Schema::Sequence*)object, output);
		}
	}
}

static void writerSchemaAnimObject(ShadyCore::Schema::Sequence* resource, std::ostream& output) {
	output.write((char*)&resource->loop, 1);
	uint32_t count = resource->frames.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto frame = resource->frames[i];
		output.write((char*)&frame->imageIndex, 19);
			// , unknown, texOffsetX/Y, texW/H, offsetX/Y, duration, renderGroup
		if (frame->hasBlendOptions()) {
			output.write((char*)&frame->blendOptions.mode, 2);
			output.write((char*)&frame->blendOptions.color, 14); // , scaleX/Y, flipV/H, angle
		}
	}
}

static void writerSchemaMoveObject(ShadyCore::Schema::Sequence* resource, std::ostream& output) {
	output.write((char*)&resource->moveLock, 5); // , actionLock, loop
	uint32_t count = resource->frames.size();
	output.write((char*)&count, 4);
	for (int i = 0; i < count; ++i) {
		auto frame = (ShadyCore::Schema::Sequence::MoveFrame*)resource->frames[i];
		output.write((char*)&frame->imageIndex, 19);
			// , unknown, texOffsetX/Y, texW/H, offsetX/Y, duration, renderGroup
		if (frame->hasBlendOptions()) {
			output.write((char*)&frame->blendOptions.mode, 2);
			output.write((char*)&frame->blendOptions.color, 14); // , scaleX/Y, flipV/H, angle
		}

		output.write((char*)&frame->traits.damage, 40);
			// , proration, chipDamage, spiritDamage, untech, power, limit
			// , onHitPlayerStun, onHitEnemyStun, onBlockPlayerStun, onBlockEnemyStun
			// , onHitCardGain, onBlockCardGain, onAirHitSetSequence, onGroundHitSetSequence
			// , speedX, speedY, onHitSfx, onHitEffect, attackLevel
		output.write((char*)&frame->traits.comboModifier, 1);
		output.write((char*)&frame->traits.frameFlags, 4);
		output.write((char*)&frame->traits.attackFlags, 4);

		uint8_t boxCount = frame->cBoxes.size();
		output.write((char*)&boxCount, 1);
		for (int i = 0; i < boxCount; ++i) {
			output.write((char*)&frame->cBoxes[i].left, 16); // , up, right, down
		}

		boxCount = frame->hBoxes.size();
		output.write((char*)&boxCount, 1);
		for (int i = 0; i < boxCount; ++i) {
			output.write((char*)&frame->hBoxes[i].left, 16); // , up, right, down
		}

		boxCount = frame->aBoxes.size();
		output.write((char*)&boxCount, 1);
		for (int i = 0; i < boxCount; ++i) {
			output.write((char*)&frame->aBoxes[i].left, 16); // , up, right, down
			if (frame->aBoxes[i].extra == 0) output.put(0);
			else {
				output.put(1);
				output.write((char*)&frame->aBoxes[i].extra->left, 16); // , up, right, down
			}
		}

		output.write((char*)&frame->effect.pivotX, 30);
			// , pivotY,  positionX/YExtra, positionX/Y, unknown02RESETSTATE, speedX/Y
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
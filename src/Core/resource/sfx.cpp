#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/riffdocument.hpp"

ShadyCore::Sfx::~Sfx() { if (data) delete[] data; }

void ShadyCore::Sfx::initialize(uint32_t s) {
	size = s;

	if (data) delete[] data;
	data = new uint8_t[size];
}

ShadyCore::LabelResource::~LabelResource() { if (name) delete[] name; }

void ShadyCore::LabelResource::initialize(const char* n) {
    size_t length = strlen(n);

    if (name) delete[] name;
    name = new char[length + 1];
    strcpy((char*)name, n);
}

static void readSfxEncrypted(ShadyCore::Sfx& resource, std::istream& input) {
}

static void writeSfxEncrypted(ShadyCore::Sfx& resource, std::ostream& output) {
}

static void readSfxDecrypted(ShadyCore::Sfx& resource, std::istream& input) {
}

static void writeSfxDecrypted(ShadyCore::Sfx& resource, std::ostream& output) {
}

void ShadyCore::ResourceDReader::accept(Sfx& resource) {
	ShadyUtil::RiffDocument riff(input);

	uint8_t buffer[16];
	riff.read("WAVEfmt ", buffer);
	resource.setChannels(*(uint16_t*)(buffer + 2));
	resource.setSampleRate(*(uint32_t*)(buffer + 4));
	resource.setByteRate(*(uint32_t*)(buffer + 8));
	resource.setBlockAlign(*(uint16_t*)(buffer + 12));
	resource.setBitsPerSample(*(uint16_t*)(buffer + 14));

	resource.initialize(riff.size("WAVEdata"));
	riff.read("WAVEdata", resource.getData());
}

void ShadyCore::ResourceEReader::accept(Sfx& resource) {
	input.ignore(2);
	ShadyUtil::readS(input, resource, &Sfx::setChannels);
	ShadyUtil::readS(input, resource, &Sfx::setSampleRate);
	ShadyUtil::readS(input, resource, &Sfx::setByteRate);
	ShadyUtil::readS(input, resource, &Sfx::setBlockAlign);
	ShadyUtil::readS(input, resource, &Sfx::setBitsPerSample);

	uint32_t size;
	input.ignore(2);
	input.read((char*)&size, 4);
	resource.initialize(size);
	input.read((char*)resource.getData(), size);
}

void ShadyCore::ResourceDWriter::accept(Sfx& resource) {
	uint32_t size = 36 + resource.getSize();
	output.write("RIFF", 4);
	ShadyUtil::writeS(output, size);
	output.write("WAVEfmt \x10\0\0\0", 12);

	output.write("\x01\0", 2);
	ShadyUtil::writeS(output, resource.getChannels());
	ShadyUtil::writeS(output, resource.getSampleRate());
	ShadyUtil::writeS(output, resource.getByteRate());
	ShadyUtil::writeS(output, resource.getBlockAlign());
	ShadyUtil::writeS(output, resource.getBitsPerSample());

	output.write("data", 4);
	ShadyUtil::writeS(output, resource.getSize());
	output.write((char*)resource.getData(), resource.getSize());
}

void ShadyCore::ResourceEWriter::accept(Sfx& resource) {
	output.write("\x01\0", 2);
	ShadyUtil::writeS(output, resource.getChannels());
	ShadyUtil::writeS(output, resource.getSampleRate());
	ShadyUtil::writeS(output, resource.getByteRate());
	ShadyUtil::writeS(output, resource.getBlockAlign());
	ShadyUtil::writeS(output, resource.getBitsPerSample());
	
	output.write("\0\0", 2);
	ShadyUtil::writeS(output, resource.getSize());
	output.write((char*)resource.getData(), resource.getSize());
}

void ShadyCore::ResourceDReader::accept(LabelResource& resource) {
	char name[32];
	double offset, size;

	input >> std::skipws >> offset >> size >> name;
	resource.initialize(name);
	resource.setOffset(offset * 44100.);
	resource.setSize(size * 44100.);
}

void ShadyCore::ResourceEReader::accept(LabelResource& resource) {
	ShadyUtil::RiffDocument riff(input);

	uint8_t buffer[32];
	riff.read("SFPLSFPI", buffer);
	resource.initialize((char*)buffer);
	riff.read("SFPLcue ", buffer, 8, 4);
	resource.setOffset(*(uint32_t*)buffer);
	riff.read("SFPLadtlltxt", buffer, 4, 4);
	resource.setSize(*(uint32_t*)buffer);
}

void ShadyCore::ResourceDWriter::accept(LabelResource& resource) {
	output << std::fixed << resource.getOffset() / 44100.0 << '\t' << resource.getSize() / 44100.0 << '\t' << resource.getName();
}

void ShadyCore::ResourceEWriter::accept(LabelResource& resource) {
	uint32_t len = 0x59 + strlen(resource.getName());
	output.write("RIFF", 4);
	ShadyUtil::writeS(output, len);

	output.write("SFPLcue \x1C\0\0\0\x01\0\0\0\x01\0\0\0", 20);
	ShadyUtil::writeS(output, resource.getOffset());
	output.write("data\0\0\0\0\0\0\0\0", 12);
	ShadyUtil::writeS(output, resource.getOffset());

	output.write("LIST\x20\0\0\0adtlltxt\x14\0\0\0\x01\0\0\0", 24);
	ShadyUtil::writeS(output, resource.getSize());

	output.write("rgn \0\0\0\0\0\0\0\0SFPI", 16);
	len = strlen(resource.getName()) + 1;
	ShadyUtil::writeS(output, len);
	output.write(resource.getName(), len);
}


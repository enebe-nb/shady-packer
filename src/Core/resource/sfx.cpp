#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/riffdocument.hpp"

#include <string>

namespace _private {
void readerSfxWave(ShadyCore::Sfx& resource, std::istream& input) {
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

void readerSfxCv(ShadyCore::Sfx& resource, std::istream& input) {
	input.ignore(2);
	ShadyUtil::readS(input, resource, &ShadyCore::Sfx::setChannels);
	ShadyUtil::readS(input, resource, &ShadyCore::Sfx::setSampleRate);
	ShadyUtil::readS(input, resource, &ShadyCore::Sfx::setByteRate);
	ShadyUtil::readS(input, resource, &ShadyCore::Sfx::setBlockAlign);
	ShadyUtil::readS(input, resource, &ShadyCore::Sfx::setBitsPerSample);

	uint32_t size;
	input.ignore(2);
	input.read((char*)&size, 4);
	resource.initialize(size);
	input.read((char*)resource.getData(), size);
}

void writerSfxWave(ShadyCore::Sfx& resource, std::ostream& output) {
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

void writerSfxCv(ShadyCore::Sfx& resource, std::ostream& output) {
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

void readerLabel(ShadyCore::LabelResource& resource, std::istream& input) {
	double offset, size;

	input.setf(std::ios::skipws);
	input >> offset >> size;
	input.unsetf(std::ios::skipws);
	resource.setOffset(offset);
	resource.setSize(size);
}

void readerLabelSfl(ShadyCore::LabelResource& resource, std::istream& input) {
	ShadyUtil::RiffDocument riff(input);
	int offset, size;

	riff.read("SFPLcue ", (uint8_t*)&offset, 8, 4);
	resource.setOffset(offset / 44100.);
	riff.read("SFPLadtlltxt", (uint8_t*)&size, 4, 4);
	resource.setSize(size / 44100.);
}

void writerLabel(ShadyCore::LabelResource& resource, std::ostream& output) {
	output << std::fixed << resource.getOffset() << '\t' << resource.getSize() << '\t' << "op2";
}

void writerLabelSfl(ShadyCore::LabelResource& resource, std::ostream& output) {
	output.write("RIFF", 4);
	ShadyUtil::writeS(output, 0x5C);

	output.write("SFPLcue \x1C\0\0\0\x01\0\0\0\x01\0\0\0", 20);
	ShadyUtil::writeS(output, (uint32_t)(resource.getOffset() * 44100.));
	output.write("data\0\0\0\0\0\0\0\0", 12);
	ShadyUtil::writeS(output, (uint32_t)(resource.getOffset() * 44100.));

	output.write("LIST\x20\0\0\0adtlltxt\x14\0\0\0\x01\0\0\0", 24);
	ShadyUtil::writeS(output, (uint32_t)(resource.getSize() * 44100.));

	output.write("rgn \0\0\0\0\0\0\0\0SFPI", 16);
	output.write("\4\0\0\0op2\0", 8);
}
}
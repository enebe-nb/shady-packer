#include "readerwriter.hpp"
#include "../util/iohelper.hpp"
#include "../util/riffdocument.hpp"

#include <string>

namespace _private {
void readerSfxWave(ShadyCore::Sfx& resource, std::istream& input) {
	ShadyUtil::RiffDocument riff(input);

	riff.read("WAVEfmt ", (uint8_t*)&resource, 0, 16);
	resource.initialize(riff.size("WAVEdata"));
	riff.read("WAVEdata", resource.data);
}

void readerSfxCv(ShadyCore::Sfx& resource, std::istream& input) {
	input.read((char*)&resource, 18);

	uint32_t size;
	input.read((char*)&size, 4);
	resource.initialize(size);
	input.read((char*)resource.data, size);
}

void writerSfxWave(ShadyCore::Sfx& resource, std::ostream& output) {
	output.write("RIFF", 4);
	ShadyUtil::writeS(output, 36 + resource.size);

	output.write("WAVEfmt \x10\0\0\0", 12);
	output.write((char*)&resource.format, 16);

	output.write("data", 4);
	ShadyUtil::writeS(output, resource.size);
	output.write((char*)resource.data, resource.size);
}

void writerSfxCv(ShadyCore::Sfx& resource, std::ostream& output) {
	output.write((char*)&resource, 18);
	ShadyUtil::writeS(output, resource.size);
	output.write((char*)resource.data, resource.size);
}

void readerLabel(ShadyCore::LabelResource& resource, std::istream& input) {
	double offset, size;

	input.setf(std::ios::skipws);
	input >> resource.begin >> resource.end;
	resource.end += resource.begin;
	input.unsetf(std::ios::skipws);
}

void readerLabelSfl(ShadyCore::LabelResource& resource, std::istream& input) {
	ShadyUtil::RiffDocument riff(input);
	int offset, size;

	riff.read("SFPLcue ", (uint8_t*)&offset, 8, 4);
	resource.begin = offset / 44100.;
	riff.read("SFPLadtlltxt", (uint8_t*)&size, 4, 4);
	resource.end = (offset + size) / 44100.;
}

void writerLabel(ShadyCore::LabelResource& resource, std::ostream& output) {
	output << std::fixed << resource.begin << '\t' << resource.end - resource.begin << '\t' << "op2";
}

void writerLabelSfl(ShadyCore::LabelResource& resource, std::ostream& output) {
	output.write("RIFF", 4);
	ShadyUtil::writeS(output, 0x5C);

	output.write("SFPLcue \x1C\0\0\0\x01\0\0\0\x01\0\0\0", 20);
	ShadyUtil::writeS(output, (uint32_t)(resource.begin * 44100.));
	output.write("data\0\0\0\0\0\0\0\0", 12);
	ShadyUtil::writeS(output, (uint32_t)(resource.begin * 44100.));

	output.write("LIST\x20\0\0\0adtlltxt\x14\0\0\0\x01\0\0\0", 24);
	ShadyUtil::writeS(output, (uint32_t)((resource.end - resource.begin) * 44100.));

	output.write("rgn \0\0\0\0\0\0\0\0SFPI", 16);
	output.write("\4\0\0\0op2\0", 8);
}
}
#include "readerwriter.hpp"
#include "../util/riffdocument.hpp"
#include <stack>

ShadyCore::TextResource::~TextResource() { if (data) delete[] data; }

void ShadyCore::TextResource::initialize(size_t l) {
	length = l;

	if (data) delete[] data;
	data = new char[length];
}

void ShadyCore::ResourceDReader::accept(TextResource& resource) {
	input.seekg(0, std::ios::end);
	resource.initialize(input.tellg());
	input.seekg(0);
	input.read(resource.getData(), resource.getLength());
}

void ShadyCore::ResourceEReader::accept(TextResource& resource) {
	char* data; size_t len;
	input.seekg(0, std::ios::end);
	resource.initialize(input.tellg());
	input.seekg(0);
	input.read(data = resource.getData(), len = resource.getLength());

	// Decrypt
	uint8_t a = 0x8b; uint8_t b = 0x71;
	while (len--) {
		(*(data++)) ^= a;
		a += b;
		b -= 0x6b;
	}
}

void ShadyCore::ResourceDWriter::accept(TextResource& resource) {
	output.write(resource.getData(), resource.getLength());
}

void ShadyCore::ResourceEWriter::accept(TextResource& resource) {
	char* data = new char[resource.getLength()];
	
	// Encrypt
	uint8_t a = 0x8b; uint8_t b = 0x71;
	for (int i = 0; i < resource.getLength(); ++i) {
		data[i] = resource.getData()[i] ^ a;
		a += b;
		b -= 0x6b;
	}
	
	output.write(data, resource.getLength());
	delete[] data;
}
#include "readerwriter.hpp"

namespace _private {
void readerText(ShadyCore::TextResource& resource, std::istream& input) {
	input.seekg(0, std::ios::end);
	resource.initialize(input.tellg());
	input.seekg(0);
	input.read(resource.data, resource.length);
}

void readerTextCv(ShadyCore::TextResource& resource, std::istream& input) {
	char* data; size_t len;
	input.seekg(0, std::ios::end);
	resource.initialize(input.tellg());
	input.seekg(0);
	input.read(data = resource.data, len = resource.length);

	// Decrypt
	uint8_t a = 0x8b; uint8_t b = 0x71;
	while (len--) {
		(*(data++)) ^= a;
		a += b;
		b -= 0x6b;
	}
}

void writerText(ShadyCore::TextResource& resource, std::ostream& output) {
	output.write(resource.data, resource.length);
}

void writerTextCv(ShadyCore::TextResource& resource, std::ostream& output) {
	char* data = new char[resource.length];
	
	// Encrypt
	uint8_t a = 0x8b; uint8_t b = 0x71;
	for (int i = 0; i < resource.length; ++i) {
		data[i] = resource.data[i] ^ a;
		a += b;
		b -= 0x6b;
	}
	
	output.write(data, resource.length);
	delete[] data;
}
}
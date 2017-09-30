#include "strallocator.hpp"
#include "riffdocument.hpp"
#include "xmlprinter.hpp"

ShadyUtil::StrAllocator::~StrAllocator() {
    for(char* str : strings) {
        delete[] str;
    }
}

char* ShadyUtil::StrAllocator::allocateString(const char* str, unsigned int size) {
    if (size) {
        strings.push_back(new char[size + 1]);
        memcpy(strings.back(), str, size);
        strings.back()[size] = '\0';
    } else {
        strings.push_back(new char[strlen(str) + 1]);
        strcpy(strings.back(), str);
    }
    return strings.back();
}

char* ShadyUtil::StrAllocator::allocateString(unsigned int size) {
    strings.push_back(new char[size + 1]);
    strings.back()[size] = '\0';
    return strings.back();
}

void ShadyUtil::StrAllocator::clear() {
	for (char* str : strings) {
		delete[] str;
	} strings.clear();
}

ShadyUtil::RiffDocument::RiffDocument(std::istream& input) : input(input) {
    typedef struct{ char type[4]; uint32_t left; } ListInfo;
	char buffer[4]; uint32_t offset = 0;
    std::vector<ListInfo> stack;

    do {
        if (input.read(buffer, 4).gcount() != 4) break;
        offset += 8;

        if (strncmp(buffer, "RIFF", 4) == 0 || strncmp(buffer, "LIST", 4) == 0) {
            stack.emplace_back();
            input.read((char*)&stack.back().left, 4);
            input.read(stack.back().type, 4);
            stack.back().left -= 4;
            offset += 4;
        } else {
            uint32_t size;
            input.read((char*)&size, 4);
            input.ignore(size);

            char* key = new char[stack.size() * 4 + 5]; int i;
            for (i = 0; i < stack.size(); ++i) memcpy(key + i * 4, stack[i].type, 4);
            memcpy(key + i++ * 4, buffer, 4);
            key[i * 4] = '\0';

            chunks[key] = DataType{ offset, size };

            stack.back().left -= size + 8;
            offset += size;
        }

        if (stack.back().left <=0 ) stack.pop_back();
    } while(stack.size());

	input.clear();
}

void ShadyUtil::RiffDocument::read(const char* key, uint8_t* buffer, uint32_t offset, uint32_t size) {
    auto i = chunks.find(key);
    if (!size) size = i->second.size;
    input.seekg(i->second.offset + offset);
    input.read((char*)buffer, size);
}

void ShadyUtil::XmlPrinter::openNode(const char* name) {
	char* nName = new char[strlen(name) + 1];
	strcpy(nName, name);
	stack.push(nName);

	if (isOpened) output << '>';
	output << '\n';
	for (uint32_t i = 0; i < indent; ++i) output << '\t';
	output << "<" << nName;
	isOpened = true;
	++indent;
}

void ShadyUtil::XmlPrinter::appendAttribute(const char* name, const char* value) {
	if (!isOpened) throw;

	output << " " << name << "=\"";
	while (*value) {
		switch (*value) {
		case '<': output << "&lt;"; break;
		case '>': output << "&gt;"; break;
		case '"': output << "&quot;"; break;
		case '&': output << "&amp;"; break;
		default: output << *value; break;
		}
		++value;
	}
	output << '"';
}

void ShadyUtil::XmlPrinter::appendContent(const char* content) {
	if (isOpened) {
		output << '>';
		isOpened = false;
	}
	
	while (content) {
		switch (*content) {
		case '<': output << "&lt;"; break;
		case '>': output << "&gt;"; break;
		case '&': output << "&amp;"; break;
		default: output << *content; break;
		}
		++content;
	}
}

void ShadyUtil::XmlPrinter::closeNode() {
	--indent;
	if (isOpened) output << "/>";
	else {
		output << '\n';
		for (uint32_t i = 0; i < indent; ++i) output << '\t';
		output << "</" << stack.top() << ">";
	}
	delete[] stack.top();
	stack.pop();
	isOpened = false;
}
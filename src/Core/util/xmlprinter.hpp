#pragma once
#include <cinttypes>
#include <ostream>
#include <stack>

namespace ShadyUtil {
    class XmlPrinter {
    private:
		std::stack<const char*> stack;
		std::ostream& output;

		uint32_t indent = 0;
		bool isOpened = false;
    public:
		inline XmlPrinter(std::ostream& output) : output(output) { output << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"; }
		inline ~XmlPrinter() { if (stack.size()) throw; }
		void openNode(const char*);
		void appendAttribute(const char*, const char*);
		inline void appendAttribute(const char* name, int8_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx8 : "%" PRIi8, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, uint8_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx8 : "%" PRIu8, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, int16_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx16 : "%" PRIi16, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, uint16_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx16 : "%" PRIu16, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, int32_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx32 : "%" PRIi32, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, uint32_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx32 : "%" PRIu32, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, int64_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx64 : "%" PRIi64, value); appendAttribute(name, buffer); }
		inline void appendAttribute(const char* name, uint64_t value, bool hex = false) { char buffer[21]; snprintf(buffer, 21, hex ? "%" PRIx64 : "%" PRIu64, value); appendAttribute(name, buffer); }
		void appendContent(const char*);
		void closeNode();
    };
}

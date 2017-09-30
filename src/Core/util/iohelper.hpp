#pragma once
#include <rapidxml.hpp>

namespace ShadyUtil {
    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(uint8_t)) {
        uint8_t buffer;
        input.read((char*)&buffer, 1);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(int8_t)) {
        int8_t buffer;
        input.read((char*)&buffer, 1);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(uint16_t)) {
        uint16_t buffer;
        input.read((char*)&buffer, 2);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(int16_t)) {
        int16_t buffer;
        input.read((char*)&buffer, 2);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(uint32_t)) {
        uint32_t buffer;
        input.read((char*)&buffer, 4);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(int32_t)) {
        int32_t buffer;
        input.read((char*)&buffer, 4);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(uint64_t)) {
        uint64_t buffer;
        input.read((char*)&buffer, 8);
        (c.*method)(buffer);
    }

    template <class C>
    inline void readS(std::istream& input, C& c, void (C::*method)(int64_t)) {
        int64_t buffer;
        input.read((char*)&buffer, 8);
        (c.*method)(buffer);
    }

	inline void writeS(std::ostream& output, const uint8_t& value) { output.write((char*)&value, 1); }
	inline void writeS(std::ostream& output, const int8_t& value) { output.write((char*)&value, 1); }
	inline void writeS(std::ostream& output, const uint16_t& value) { output.write((char*)&value, 2); }
	inline void writeS(std::ostream& output, const int16_t& value) { output.write((char*)&value, 2); }
	inline void writeS(std::ostream& output, const uint32_t& value) { output.write((char*)&value, 4); }
	inline void writeS(std::ostream& output, const int32_t& value) { output.write((char*)&value, 4); }
	inline void writeS(std::ostream& output, const uint64_t& value) { output.write((char*)&value, 8); }
	inline void writeS(std::ostream& output, const int64_t& value) { output.write((char*)&value, 8); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(int8_t), bool hex = false) {
		(c.*method)(strtol(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(uint8_t), bool hex = false) {
		(c.*method)(strtoul(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(int16_t), bool hex = false) {
		(c.*method)(strtol(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(uint16_t), bool hex = false) {
		(c.*method)(strtoul(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(int32_t), bool hex = false) {
		(c.*method)(strtol(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(uint32_t), bool hex = false) {
		(c.*method)(strtoul(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(int64_t), bool hex = false) {
		(c.*method)(strtoll(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }

	template <class C>
	inline void readX(rapidxml::xml_node<>* node, const char* attName, C& c, void (C::*method)(uint64_t), bool hex = false) {
		(c.*method)(strtoull(node->first_attribute(attName)->value(), 0, hex ? 16 : 10)); }
}

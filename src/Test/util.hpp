#pragma once
#include <cxxtest/TestSuite.h>

#define TS_ASSERT_STREAM(S1, S2) { \
    char _b1[16], _b2[16]; std::streamsize _s; \
    do { \
		TS_ASSERT_EQUALS(_s = S1.read(_b1, 16).gcount(), S2.read(_b2, 16).gcount()); \
		TS_ASSERT_SAME_DATA(_b1, _b2, _s); \
    } while(_s); \
}

#define TSM_ASSERT_STREAM(MSG, S1, S2) { \
    char _b1[16], _b2[16], _e[128]; std::streamsize _s; int _o = 0; \
    do { \
		TSM_ASSERT_EQUALS(MSG, _s = S1.read(_b1, 16).gcount(), S2.read(_b2, 16).gcount()); \
		sprintf(_e, "%s: Data not equal starting in 0x%08x offset.", MSG, _o); \
		TSM_ASSERT_SAME_DATA(_e, _b1, _b2, _s); \
		_o += _s; \
    } while(_s); \
}

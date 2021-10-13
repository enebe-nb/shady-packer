#pragma once

#define ASSERT_STREAM(S1, S2) { \
	char _b1[16], _b2[16]; std::streamsize _s; \
	do { \
		EXPECT_EQ(_s = S1.read(_b1, 16).gcount(), S2.read(_b2, 16).gcount()); \
		EXPECT_TRUE(memcmp(_b1, _b2, _s) == 0); \
	} while(_s); \
}

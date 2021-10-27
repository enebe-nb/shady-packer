#include "util.hpp"

testing::AssertionResult testing::isSameData(std::istream& l, std::istream& r) {
    char lb[16], rb[16], mb[_MAX_ITOSTR_BASE16_COUNT];
    std::streamsize lc, rc, offset = 0;
    do {
        lc = l.read(lb, 16).gcount(); rc = r.read(rb, 16).gcount();
        if (lc != rc) return AssertionFailure()
            << "size mismatch: " << offset + lc << " != " << offset + rc;
        if (memcmp(lb, rb, lc) != 0) return AssertionFailure()
            << "data mismatch [0x"<<itoa(offset, mb, 16)<<"]: " << PrintToString(lb) << " != " << PrintToString(rb) << " " << lc;
        offset += lc;
    } while(lc);
    return AssertionSuccess() << "size: " << offset;
}
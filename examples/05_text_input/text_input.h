#pragma once
#include <stddef.h>
#include <stdint.h>

// Complete UTF-8 scalar values only: excludes overlong forms, surrogates and embedded controls.
inline bool validText(const char *text, size_t n) {
    bool visible = false;
    for (size_t i = 0; i < n;) {
        uint32_t cp = (uint8_t)text[i++];
        unsigned extra = 0;
        uint32_t minimum = 0;
        if (cp >= 0xc2 && cp <= 0xdf) { cp &= 31; extra = 1; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { cp &= 15; extra = 2; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; extra = 3; minimum = 0x10000; }
        else if (cp >= 0x80) return false;
        if (extra > n - i) return false;
        while (extra--) {
            const uint8_t b = (uint8_t)text[i++];
            if ((b & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (b & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ||
            cp < 0x20 || (cp >= 0x7f && cp <= 0x9f)) return false;
        if (cp != 0x20 && cp != 0x3000) visible = true;
    }
    return visible;
}

class TextLine {
    char bytes[1024] = {};
    size_t used = 0;
    bool overflow = false;
    bool afterCR = false;
public:
    enum Result { More, Complete, Rejected };
    void reset() { used = 0; overflow = false; afterCR = false; }
    const char *text() const { return bytes; }
    Result push(uint8_t ch) {
        if (afterCR && ch == '\n') { afterCR = false; return More; }
        afterCR = false;
        if (ch != '\r' && ch != '\n') {
            if (used < sizeof(bytes) - 1) bytes[used++] = (char)ch;
            else overflow = true;
            return More;
        }
        afterCR = ch == '\r';
        bytes[used] = 0;
        const bool ok = !overflow && validText(bytes, used);
        used = 0;
        overflow = false;
        return ok ? Complete : Rejected;
    }
};

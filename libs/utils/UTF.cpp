//
// Created by TangRuiwen on 2020/10/11.
//

#include <utils/UTF.h>

#include <climits>

namespace txt {

static constexpr inline int32_t left_shift(int32_t value, int32_t shift) {
    return static_cast<int32_t>(static_cast<uint32_t>(value) << shift);
}

template <typename T>
static constexpr bool is_align2(T x) {
    return 0 == (x & 1);
}

template <typename T>
static constexpr bool is_align4(T x) {
    return 0 == (x & 3);
}

template <typename T>
static int32_t next_fail(const T** ptr, const T* end) {
    *ptr = end;
    return -1;
}

static constexpr inline bool utf16_is_high_surrogate(uint16_t c) {
    return (c & 0xFC00) == 0xD800;
}

static constexpr inline bool utf16_is_low_surrogate(uint16_t c) {
    return (c & 0xFC00) == 0xDC00;
}

/** @returns   -1  iff invalid UTF8 byte,
                0  iff UTF8 continuation byte,
                1  iff ASCII byte,
                2  iff leading byte of 2-byte sequence,
                3  iff leading byte of 3-byte sequence, and
                4  iff leading byte of 4-byte sequence.
      I.e.: if return value > 0, then gives length of sequence.
*/
static int utf8_byte_type(uint8_t c) {
    if (c < 0x80) {
        return 1;
    } else if (c < 0xC0) {
        return 0;
    } else if (c >= 0xF5 || (c & 0xFE) == 0xC0) { // "octet values c0, c1, f5 to ff never appear"
        return -1;
    } else {
        int value = (((0xe5 << 24) >> ((unsigned)c >> 4 << 1)) & 3) + 1;
        // assert(value >= 2 && value <=4);
        return value;
    }
}

static bool utf8_type_is_valid_leading_byte(int type) {
    return type > 0;
}

static bool utf8_byte_is_continuation(uint8_t c) {
    return utf8_byte_type(c) == 0;
}

int32_t UTF::CountUTF8(const char* utf8, size_t byteLength) {
    if (!utf8) {
        return -1;
    }
    int32_t count = 0;
    const char* stop = utf8 + byteLength;
    while (utf8 < stop) {
        int type = utf8_byte_type(*(const uint8_t*)utf8);
        if (!utf8_type_is_valid_leading_byte(type) || utf8 + type > stop) {
            return -1; // Sequence extends beyond end.
        }
        while (type-- > 1) {
            ++utf8;
            if (!utf8_byte_is_continuation(*(const uint8_t*)utf8)) {
                return -1;
            }
        }
        ++utf8;
        ++count;
    }
    return count;
}

int32_t UTF::CountUTF16(const uint16_t* utf16, size_t byteLength) {
    if (!utf16 || !is_align2(intptr_t(utf16)) || !is_align2(byteLength)) {
        return -1;
    }
    const uint16_t* src = (const uint16_t*)utf16;
    const uint16_t* stop = src + (byteLength >> 1);
    int count = 0;
    while (src < stop) {
        unsigned c = *src++;
        if (utf16_is_low_surrogate(c)) {
            return -1;
        }
        if (utf16_is_high_surrogate(c)) {
            if (src >= stop) {
                return -1;
            }
            c = *src++;
            if (!utf16_is_low_surrogate(c)) {
                return -1;
            }
        }
        count += 1;
    }
    return count;
}

int32_t UTF::NextUTF8(const char** ptr, const char* end) {
    if (!ptr || !end) {
        return -1;
    }

    const uint8_t* p = (const uint8_t*)*ptr;
    if (!p || p >= (const uint8_t*)end) {
        return next_fail(ptr, end);
    }

    int32_t c = *p;
    int32_t hic = c << 24;
    if (!utf8_type_is_valid_leading_byte(utf8_byte_type(c))) {
        return next_fail(ptr, end);
    }

    if (hic < 0) {
        uint32_t mask = (uint32_t)~0x3F;
        hic = left_shift(hic, 1);
        do {
            ++p;
            if (p >= (const uint8_t*)end) {
                return next_fail(ptr, end);
            }
            // check before reading off end of array.
            uint8_t nextByte = *p;
            if (!utf8_byte_is_continuation(nextByte)) {
                return next_fail(ptr, end);
            }
            c = (c << 6) | (nextByte & 0x3F);
            mask <<= 5;
        } while ((hic = left_shift(hic, 1)) < 0);
        c &= ~mask;
    }
    *ptr = (char*)p + 1;
    return c;
}

int32_t UTF::NextUTF16(const uint16_t** ptr, const uint16_t* end) {
    if (!ptr || !end) {
        return -1;
    }
    const uint16_t* src = *ptr;
    if (!src || src + 1 > end || !is_align2(intptr_t(src))) {
        return next_fail(ptr, end);
    }
    uint16_t c = *src++;
    int32_t result = c;
    if (utf16_is_low_surrogate(c)) {
        return next_fail(ptr, end); // srcPtr should never point at low surrogate.
    }
    if (utf16_is_high_surrogate(c)) {
        if (src + 1 > end) {
            return next_fail(ptr, end); // Truncated string.
        }
        uint16_t low = *src++;
        if (!utf16_is_low_surrogate(low)) {
            return next_fail(ptr, end);
        }
        /*
        [paraphrased from wikipedia]
        Take the high surrogate and subtract 0xD800, then multiply by 0x400.
        Take the low surrogate and subtract 0xDC00.  Add these two results
        together, and finally add 0x10000 to get the final decoded codepoint.

        unicode = (high - 0xD800) * 0x400 + low - 0xDC00 + 0x10000
        unicode = (high * 0x400) - (0xD800 * 0x400) + low - 0xDC00 + 0x10000
        unicode = (high << 10) - (0xD800 << 10) + low - 0xDC00 + 0x10000
        unicode = (high << 10) + low - ((0xD800 << 10) + 0xDC00 - 0x10000)
        */
        result = (result << 10) + (int32_t)low - ((0xD800 << 10) + 0xDC00 - 0x10000);
    }
    *ptr = src;
    return result;
}

} // namespace txt
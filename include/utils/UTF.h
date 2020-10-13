//
// Created by TangRuiwen on 2020/10/11.
//

#ifndef INCLUDE_LIBS_UTF_H_
#define INCLUDE_LIBS_UTF_H_

#include <cstddef>
#include <cstdint>

namespace txt {

class UTF {
public:
    /**
     *  Given a sequence of UTF-8 bytes, return the number of unicode codepoints..
     *
     * \param utf8
     * \param byteLength
     * \return If invalid UTF-8, return -1
     */
    static int32_t CountUTF8(const char* utf8, size_t byteLength);

    /**
     * . Given a sequence of UTF-16 bytes, return the number of unicode codepoints.
     *
     * \param utf16
     * \param byteLength
     * \return If invalid UTF-16, return -1
     */
    static int32_t CountUTF16(const uint16_t* utf16, size_t byteLength);

    /**
     * .Given a sequence of aligned UTF-8 characters in machine-endian form, return the first
     * unicode codepoint.
     *
     * \param ptr
     * \param end
     * \return -1 if invalid UTF-8 is encountered
     */
    static int32_t NextUTF8(const char** ptr, const char* end);

    /**
     * .Given a sequence of aligned UTF-16 characters in machine-endian form, return the first
     * unidoce codepoint.
     *
     * \param ptr
     * \param end
     * \return -1 if invalid UTF-8 is encountered
     */
    static int32_t NextUTF16(const uint16_t** ptr, const uint16_t* end);
};

} // namespace txt

#endif
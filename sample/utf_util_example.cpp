//
// Created by TangRuiwen on 2020/10/11.
//

#include <utils/UTF.h>

#include <iostream>
#include <string>

int main(int argc, const char** argv) {
    char text[] = 
    // "fine world \xe4\xb8\xad";
    "fine world \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87";

    int count = txt::UTF::CountUTF8(text, sizeof(text));
    std::cout << "raw text = " << text << std::endl;
    std::cout << "count = " << count << std::endl;

    const char* ptr = text;
    const char* end = text + sizeof(text);
    const char* p = ptr;
    while (ptr != end) {
        int32_t ch = txt::UTF::NextUTF8(&ptr, end);
        uint32_t length = ptr - p;
        std::string char_str{p, length};
        std::cout << "next ch = " << char_str << std::endl;
        p = ptr;
    }

    return 0;
}
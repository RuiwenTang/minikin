//
// Created by TangRuiwen on 2020/9/29.
//

#include <minikin/Layout.h>
#include <minikin/MinikinFontCoreText.h>
#include <unicode/unistr.h>

#include <iostream>
#include <string>
#include <vector>

static android::FontCollection* makeFontCollection() {
    auto family = new android::FontFamily;
    std::vector<android::FontFamily*> typefaces;
    std::vector<std::string> fns{"Roboto-Regular",    "Roboto-Italic",     "Roboto-BoldItalic",
                                 "Roboto-Light",      "Roboto-Thin",       "Roboto-Bold",
                                 "Roboto-ThinItalic", "Roboto-LightItalic"};

    for (const auto& name : fns) {
        std::cout << "adding " << name << std::endl;
        CTFontRef ct_font =
                CTFontCreateWithName(CFStringCreateWithCString(CFAllocatorGetDefault(),
                                                               name.c_str(),
                                                               CFStringEncodings::
                                                                       kCFStringEncodingGB_2312_80),
                                     40, nullptr);
        if (!ct_font) {
            std::cout << "can not load font " << name << std::endl;
            continue;
        }

        android::MinikinFont* minikin_font = new minikin::MinikinFontCoreText(ct_font);
        family->addFont(minikin_font);
    }
    typefaces.push_back(family);

    return new android::FontCollection(typefaces);
}

int main(int argc, const char** argv) {
    const auto collector = makeFontCollection();

    android::Layout::init();

    android::Layout layout;
    layout.setFontCollection(collector);

    android::FontStyle font_style;
    android::MinikinPaint paint;
    paint.size = 32;
    int bidi_falgs = 0;
    const char* text =
            "fine world \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87";
    icu::UnicodeString icu_text = icu::UnicodeString::fromUTF8(text);
    layout.doLayout(reinterpret_cast<const uint16_t*>(icu_text.getBuffer()), 0, icu_text.length(),
                    icu_text.length(), bidi_falgs, font_style, paint);

    layout.dump();

    float width =
            android::Layout::measureText(reinterpret_cast<const uint16_t*>(icu_text.getBuffer()), 0,
                                         icu_text.length(), icu_text.length(), bidi_falgs,
                                         font_style, paint, collector, (float*)nullptr);
    std::cout << "width = " << width << std::endl;
    return 0;
}
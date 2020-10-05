//
// Created by TangRuiwen on 2020/9/29.
//

#include <minikin/Layout.h>
#include <minikin/MinikinFontCoreText.h>
#include <unicode/unistr.h>

#include <iostream>
#include <string>
#include <vector>

static minikin::FontCollection* makeFontCollection() {
    auto family = std::make_shared<minikin::FontFamily>();
    std::vector<std::shared_ptr<minikin::FontFamily>> typefaces;
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
                                     20, nullptr);
        if (!ct_font) {
            std::cout << "can not load font " << name << std::endl;
            continue;
        }
        std::shared_ptr<minikin::MinikinFont> minikin_font =
                std::make_shared<minikin::MinikinFontCoreText>(ct_font);
        family->addFont(minikin_font);
    }
    typefaces.push_back(family);

    return new minikin::FontCollection(typefaces);
}

int main(int argc, const char** argv) {
    const auto collector = makeFontCollection();

    minikin::Layout::init();

    minikin::Layout layout;
    layout.setFontCollection(collector);

    minikin::FontStyle font_style;
    minikin::MinikinPaint paint;
    paint.size = 20;
    int bidi_falgs = 0;
    const char* text =
            "fine world \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87";
    icu::UnicodeString icu_text = icu::UnicodeString::fromUTF8(text);
    layout.doLayout(reinterpret_cast<const uint16_t*>(icu_text.getBuffer()), 0, icu_text.length(),
                    icu_text.length(), bidi_falgs, font_style, paint);

    layout.dump();

    float width =
            minikin::Layout::measureText(reinterpret_cast<const uint16_t*>(icu_text.getBuffer()), 0,
                                         icu_text.length(), icu_text.length(), bidi_falgs,
                                         font_style, paint, collector, (float*)nullptr);
    std::cout << "width = " << width << std::endl;
    minikin::MinikinRect rect;
    layout.getBounds(&rect);
    std::cout << "rect = {" << rect.mLeft << ", " << rect.mTop << ", " << rect.mRight << ", "
              << rect.mBottom << "}" << std::endl;
    return 0;
}
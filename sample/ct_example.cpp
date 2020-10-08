//
// Created by TangRuiwen on 2020/9/29.
//

#include <minikin/Layout.h>
#include <minikin/MinikinFontCoreText.h>
#include <unicode/unistr.h>

#include <iostream>
#include <string>
#include <vector>

#include "SampleConfig.h"

static minikin::FontCollection* makeFontCollection() {
    auto family = std::make_shared<minikin::FontFamily>();
    std::vector<std::shared_ptr<minikin::FontFamily>> typefaces;
    std::vector<std::string> fns{FONT_DIR "/Roboto-Regular.ttf",
                                 FONT_DIR "/Roboto-Italic.ttf",
                                 FONT_DIR "/Roboto-BoldItalic.ttf",
                                 FONT_DIR "/Roboto-Light.ttf",
                                 FONT_DIR "/Roboto-Thin.ttf",
                                 FONT_DIR "/Roboto-Bold.ttf",
                                 FONT_DIR "/Roboto-ThinItalic.ttf",
                                 FONT_DIR "/Roboto-LightItalic.ttf"};

    for (const auto& name : fns) {
        std::cout << "adding " << name << std::endl;
        CFURLRef url =
                CFURLCreateWithFileSystemPath(CFAllocatorGetDefault(),
                                              CFStringCreateWithCString(CFAllocatorGetDefault(),
                                                                        name.c_str(),
                                                                        kCFStringEncodingUTF8),
                                              kCFURLPOSIXPathStyle, false);
        CFArrayRef fd = CTFontManagerCreateFontDescriptorsFromURL(url);
        CTFontRef ct_font =
                CTFontCreateWithFontDescriptor((CTFontDescriptorRef)CFArrayGetValueAtIndex(fd, 0),
                                               0, nullptr);
        CFRelease(url);
        CFRelease(fd);
        if (!ct_font) {
            std::cout << "can not load font " << name << std::endl;
            continue;
        }
        std::shared_ptr<minikin::MinikinFont> minikin_font =
                std::make_shared<minikin::MinikinFontCoreText>(ct_font);
        family->addFont(minikin_font);
        CFRelease(ct_font);
    }
    typefaces.push_back(family);

#if 1
    family = std::make_shared<minikin::FontFamily>();
    CFURLRef url = CFURLCreateWithFileSystemPath(CFAllocatorGetDefault(),
                                                 CFSTR(FONT_DIR "/DroidSansDevanagari-Regular.ttf"),
                                                 kCFURLPOSIXPathStyle, false);
    CFArrayRef fd = CTFontManagerCreateFontDescriptorsFromURL(url);
    CTFontRef ct_font =
            CTFontCreateWithFontDescriptor((CTFontDescriptorRef)CFArrayGetValueAtIndex(fd, 0), 0,
                                           nullptr);
    assert(ct_font != nullptr);
    std::shared_ptr<minikin::MinikinFont> append_font =
            std::make_shared<minikin::MinikinFontCoreText>(ct_font);
    family->addFont(append_font);
    typefaces.push_back(family);
    CFRelease(url);
    CFRelease(fd);
    CFRelease(ct_font);
#endif

    return new minikin::FontCollection(typefaces);
}

int main(int argc, const char** argv) {
    const auto collector = makeFontCollection();

    minikin::Layout::init();

    minikin::Layout layout;
    layout.setFontCollection(collector);

    minikin::FontStyle font_style;
    minikin::MinikinPaint paint;
    paint.size = 30;
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
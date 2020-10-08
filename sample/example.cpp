/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This is a test program that uses Minikin to layout and draw some text.
// At the moment, it just draws a string into /data/local/tmp/foo.pgm.

#include <minikin/Layout.h>
#include <minikin/MinikinFontFreeType.h>
#include <stdio.h>
#include <unicode/unistr.h>
#include <unicode/utf16.h>

#ifndef ANDROID
#include "SampleConfig.h"
#endif

#include <fstream>
#include <vector>

using std::vector;
using namespace minikin;
using namespace minikin;

FT_Library library; // TODO: this should not be a global

FontCollection *makeFontCollection() {
    vector<std::shared_ptr<FontFamily>> typefaces;
#ifdef ANDROID
    const char *fns[] = {"/system/fonts/Roboto-Regular.ttf",
                         "/system/fonts/Roboto-Italic.ttf",
                         "/system/fonts/Roboto-BoldItalic.ttf",
                         "/system/fonts/Roboto-Light.ttf",
                         "/system/fonts/Roboto-Thin.ttf",
                         "/system/fonts/Roboto-Bold.ttf",
                         "/system/fonts/Roboto-ThinItalic.ttf",
                         "/system/fonts/Roboto-LightItalic.ttf"};
#else
    const char *fns[] = {FONT_DIR "/Roboto-Regular.ttf",    FONT_DIR "/Roboto-Italic.ttf",
                         FONT_DIR "/Roboto-BoldItalic.ttf", FONT_DIR "/Roboto-Light.ttf",
                         FONT_DIR "/Roboto-Thin.ttf",       FONT_DIR "/Roboto-Bold.ttf",
                         FONT_DIR "/Roboto-ThinItalic.ttf", FONT_DIR "/Roboto-LightItalic.ttf"};
#endif
    auto family = std::make_shared<FontFamily>();
    FT_Face face;
    FT_Error error;
    for (size_t i = 0; i < 8; i++) {
        const char *fn = fns[i];
        printf("adding %s\n", fn);
        error = FT_New_Face(library, fn, 0, &face);
        if (error != 0) {
            printf("error loading %s, %d\n", fn, error);
        }
        auto font = std::make_shared<MinikinFontFreeType>(face);
        family->addFont(font);
    }
    typefaces.push_back(family);

#if 1
    family = std::make_shared<FontFamily>();
#ifdef ANDROID
    const char *fn = "/system/fonts/DroidSansDevanagari-Regular.ttf";
#else
    const char *fn = FONT_DIR "/DroidSansDevanagari-Regular.ttf";
#endif
    error = FT_New_Face(library, fn, 0, &face);
    auto font = std::make_shared<MinikinFontFreeType>(face);
    family->addFont(font);
    typefaces.push_back(family);
#endif

#if 0
#ifndef ANDROID
    family = std::make_shared<FontFamily>();
    const char *emoji = FONT_DIR "/NotoColorEmoji.ttf";
    error = FT_New_Face(library, emoji, 0, &face);
    auto font1 = std::make_shared<MinikinFontFreeType>(face);
    family->addFont(font1);
    typefaces.push_back(family);
#endif
#endif

    return new FontCollection(typefaces);
}

int runMinikinTest() {
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        return -1;
    }
    Layout::init();

    FontCollection *collection = makeFontCollection();
    Layout layout;
    layout.setFontCollection(collection);
    const char *text =
            "fine world \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87";
    int bidiFlags = 0;
    FontStyle fontStyle;
    MinikinPaint paint;
    paint.size = 30;
    icu::UnicodeString icuText = icu::UnicodeString::fromUTF8(text);
    layout.doLayout(reinterpret_cast<const uint16_t *>(icuText.getBuffer()), 0, icuText.length(),
                    icuText.length(), bidiFlags, fontStyle, paint);
    layout.dump();
    Bitmap bitmap(250, 50);
    MinikinRect rect;
    layout.getBounds(&rect);
    printf("rect = {%f, %f, %f, %f}\n", rect.mLeft, rect.mTop, rect.mRight, rect.mBottom);
    layout.draw(&bitmap, 10, 40, paint.size);
    std::ofstream o;
    o.open("foo.pgm", std::ios::out | std::ios::binary);
    bitmap.writePnm(o);
    return 0;
}

int main() {
    return runMinikinTest();
}

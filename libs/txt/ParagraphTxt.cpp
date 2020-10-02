//
// Created by TangRuiwen on 2020/10/02.
//

#include "ParagraphTxt.h"

#include <hb.h>
#include <minikin/Layout.h>

#include "../minikin/FontLanguageListCache.h"
#include "../minikin/LayoutUtils.h"

namespace txt {

class GlyphTypeface {
public:
    GlyphTypeface(hb_blob_t* raw_font, minikin::FontFakery fakery)
          : raw_font_(raw_font),
            fake_bold_(fakery.isFakeBold()),
            fake_italic_(fakery.isFakeItalic()) {}

    bool operator==(GlyphTypeface& other) {
        return raw_font_ == other.raw_font_ && fake_bold_ == other.fake_bold_ &&
                fake_italic_ == other.fake_italic_;
    }

    bool operator!=(GlyphTypeface& other) { return !(*this == other); }

private:
    hb_blob_t* raw_font_;
    bool fake_bold_;
    bool fake_italic_;
};

static GlyphTypeface GetGlyphTypeface(const minikin::Layout& layout, size_t index) {
    minikin::MinikinFont* font = layout.getFont(index);
    return GlyphTypeface{(hb_blob_t*)(font->GetFontData()), layout.getFakery(index)};
}

// Return ranges of text that have the same typeface in the layout.
static std::vector<Paragraph::Range<size_t>> GetLayoutTypefaceRuns(const minikin::Layout& layout) {
    std::vector<Paragraph::Range<size_t>> result;
    if (layout.nGlyphs() == 0) {
        return result;
    }

    size_t run_start = 0;
    GlyphTypeface run_typeface = GetGlyphTypeface(layout, run_start);
    for (size_t i = 1; i < layout.nGlyphs(); i++) {
        GlyphTypeface typeface = GetGlyphTypeface(layout, i);
        if (typeface != run_typeface) {
            result.emplace_back(run_start, i);
            run_start = i;
            run_typeface = typeface;
        }
    }

    result.emplace_back(run_start, layout.nGlyphs());
    return result;
}

static int GetWeight(const FontWeight weight) {
    switch (weight) {
        case FontWeight::w100:
            return 1;
        case FontWeight::w200:
            return 2;
        case FontWeight::w300:
            return 3;
        case FontWeight::w400: // Normal.
            return 4;
        case FontWeight::w500:
            return 5;
        case FontWeight::w600:
            return 6;
        case FontWeight::w700: // Bold.
            return 7;
        case FontWeight::w800:
            return 8;
        case FontWeight::w900:
            return 9;
        default:
            return -1;
    }
}

static int GetWeight(const TextStyle& style) {
    return GetWeight(style.font_weight);
}

static bool GetItalic(const TextStyle& style) {
    switch (style.font_style) {
        case FontStyle::Italic:
            return true;
        case FontStyle::Normal:
        default:
            return false;
    }
}

static minikin::FontStyle GetMinikinFontStyle(const TextStyle& style) {
    uint32_t language_list_id = style.local.empty()
            ? minikin::FontLanguageListCache::kEmptyListId
            : minikin::FontStyle::registerLanguageList(style.local);
    return minikin::FontStyle(language_list_id, 0, GetWeight(style), GetItalic(style));
}

static void GetFontAndMinikinPaint(const TextStyle& style, minikin::FontStyle* font,
                                   minikin::MinikinPaint* paint) {
    *font = GetMinikinFontStyle(style);
    paint->size = style.font_size;
    // Divide by font size so letter spacing is pixels, not proprotional to font size.
    paint->letterSpacing = style.letter_spacing / style.font_size;
    // TODO append
    // paint->wordSpacing = style.word_spacing;
    paint->scaleX = 1.0f;
    // Prevent spacing rounding in Minikin. This causes jitter when switching
    // between same text content with different runs composing it, however, it
    // also produces more accurate layouts.
    paint->paintFlags |= minikin::LinearTextFlag;
    // TODO append
    // paint->fontFeatureSettings = style.font_features.GetFeatureStrings();
}

static void FindWords(const std::vector<uint16_t>& text, size_t start, size_t end,
                      std::vector<Paragraph::Range<size_t>>& words) {
    bool in_word = false;
    size_t word_start;
    for (size_t i = start; i < end; i++) {
        bool is_space = minikin::isWordSpace(text[i]);
        if (!in_word && !is_space) {
            word_start = i;
            in_word = true;
        } else {
            words.emplace_back(word_start, i);
            in_word = false;
        }
    }
    if (in_word) {
        words.emplace_back(word_start, end);
    }
}

static const float constexpr kDoubleDecorationSpacing = 3.0f;

ParagraphTxt::GlyphPosition::GlyphPosition(double x_start, double x_advance, size_t code_unit_index,
                                           size_t code_unit_width, size_t cluster)
      : code_units(code_unit_index, code_unit_index + code_unit_width),
        x_pos(x_start, x_start + x_advance),
        cluster(cluster) {}

void ParagraphTxt::GlyphPosition::Shift(double delta) {
    x_pos.Shift(delta);
}

} // namespace txt
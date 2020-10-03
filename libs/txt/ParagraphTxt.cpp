//
// Created by TangRuiwen on 2020/10/02.
//

#include "ParagraphTxt.h"

#include <hb.h>
#include <minikin/Layout.h>
#include <unicode/ubidi.h>

#include <cstring>

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

ParagraphTxt::GlyphLine::GlyphLine(std::vector<GlyphPosition>&& positions, size_t total_code_units)
      : positions(std::move(positions)), total_code_units(total_code_units) {}

ParagraphTxt::CodeUnitRun::CodeUnitRun(std::vector<GlyphPosition>&& positions, Range<size_t> cu,
                                       Range<double> x_pos, size_t line, const TextStyle& style,
                                       TextDirection direction, const PlaceholderRun* placeholder)
      : positions(std::move(positions)),
        code_units(cu),
        x_pos(x_pos),
        line_number(line),
        style(std::addressof(style)),
        direction(direction),
        placeholder_run(placeholder) {}

void ParagraphTxt::CodeUnitRun::Shift(double delta) {
    x_pos.Shift(delta);
    for (auto& position : positions) {
        position.Shift(delta);
    }
}

ParagraphTxt::ParagraphTxt()
      : did_exceed_max_lines_(false), final_line_count_(0), max_right_(0), min_left_(0) {
    breaker_.setLocale(icu::Locale(), nullptr);
}

ParagraphTxt::~ParagraphTxt() = default;

void ParagraphTxt::SetText(std::vector<uint16_t> text, StyledRuns runs) {
    this->SetDirty(true);
    if (text.empty()) {
        return;
    }

    this->text_ = std::move(text);
    this->runs_ = std::move(runs);
}

void ParagraphTxt::SetInlinePlaceholders(std::vector<PlaceholderRun> inline_placeholders,
                                         std::unordered_set<size_t> obj_replacement_char_indexes) {
    needs_layout_ = true;
    this->inline_placeholders_ = std::move(inline_placeholders);
    this->obj_replacement_char_indexes_ = std::move(obj_replacement_char_indexes);
}

bool ParagraphTxt::ComputeLineBreaks() {
    line_metrics_.clear();
    line_widths_.clear();
    max_intrinsic_width_ = 0;

    std::vector<size_t> newline_positions;
    // Discover and add all hard breaks.
    for (size_t i = 0; i < text_.size(); i++) {
        ULineBreak ulb = static_cast<ULineBreak>(u_getIntPropertyValue(text_[i], UCHAR_LINE_BREAK));
        if (ulb == U_LB_LINE_FEED || ulb == U_LB_MANDATORY_BREAK) {
            newline_positions.emplace_back(i);
        }
    }
    // Break at the end of the paragraph.
    newline_positions.emplace_back(text_.size());

    // Calculate and add any breaks due to a line being too long.
    size_t run_index = 0;
    size_t inline_placeholder_index = 0;
    for (size_t newline_index = 0; newline_index < newline_positions.size(); newline_index++) {
        size_t block_start = (newline_index > 0) ? newline_positions[newline_index - 1] + 1 : 0;
        size_t block_end = newline_positions[newline_index];
        size_t block_size = block_end - block_start;

        if (block_size == 0) {
            line_metrics_.emplace_back(block_start, block_end, block_end, block_end + 1, true);
            line_widths_.emplace_back(0);
            continue;
        }

        // Setup breaker. We wait to set the line width in order to account for the widths of the
        // inline placeholders, which are calculated in the loop over the runs.
        breaker_.setLineWidths(0.f, 0, width_);
        breaker_.setJustified(paragraph_style_.text_align == TextAlign::kJustify);
        breaker_.setStrategy(paragraph_style_.break_strategy);
        breaker_.resize(block_size);
        std::memcpy(breaker_.buffer(), text_.data() + block_start, block_size * sizeof(text_[0]));
        breaker_.setText();

        // Add the runs that include this line to the LineBreaker.
        double block_total_width = 0;
        while (run_index < runs_.Size()) {
            StyledRuns::Run run = runs_.GetRun(run_index);
            if (run.start >= block_end) {
                break;
            }
            if (run.end < block_start) {
                run_index++;
                continue;
            }
            minikin::FontStyle font;
            minikin::MinikinPaint paint;
            GetFontAndMinikinPaint(run.style, std::addressof(font), std::addressof(paint));
            std::shared_ptr<minikin::FontCollection> collection =
                    GetMinikinFontCollectionForStyle(run.style);
            if (!collection) {
                // Could not find font collection for families "run.style.font_families[0]"
                return false;
            }

            size_t run_start = std::max(run.start, block_start) - block_start;
            size_t run_end = std::min(run.end, block_end) - block_start;
            bool is_rtl = (paragraph_style_.text_direction == TextDirection::kRtl);

            // Check if the run is an object replacement character-only run. We should leave space
            // for inline placeholder and break around it if appropriate.
            if (run.end - run.start == 1 && obj_replacement_char_indexes_.count(run.start) != 0 &&
                text_[run.start] == objReplacementChar &&
                inline_placeholder_index < inline_placeholders_.size()) {
                // Is a inline placeholder run.
                PlaceholderRun placeholder_run = inline_placeholders_[inline_placeholder_index];
                block_total_width += placeholder_run.width;

                // TODO implement
                // Inject custom width into minikin breaker. (Uses LibTxt-minikin patch).
                // breaker_.setCustomCharWidth(run_start, placeholder_run.width);

                // Called with nullptr as paint in order to use the custom widths passed above.
                breaker_.addStyleRun(nullptr, collection.get(), font, run_start, run_end, is_rtl);
                inline_placeholder_index++;
            } else {
                // Is a regular text run.
                double run_width = breaker_.addStyleRun(std::addressof(paint), collection.get(),
                                                        font, run_start, run_end, is_rtl);
                block_total_width += run_width;
            }

            if (run.end > block_end) {
                break;
            }
            run_index++;
        }
        max_intrinsic_width_ = std::max(max_intrinsic_width_, block_total_width);

        size_t breaks_count = breaker_.computeBreaks();
        const int* breaks = breaker_.getBreaks();
        for (size_t i = 0; i < breaks_count; i++) {
            size_t break_start = (i > 0) ? breaks[i - 1] : 0;
            size_t line_start = break_start + block_start;
            size_t line_end = breaks[i] + block_start;
            bool hard_break = i == breaks_count - 1;
            size_t line_end_including_newline =
                    (hard_break && line_end < text_.size()) ? line_end + 1 : line_end;
            size_t line_end_excluding_whitespace = line_end;
            while (line_end_excluding_whitespace > line_start &&
                   minikin::isLineEndSpace(text_[line_end_excluding_whitespace - 1])) {
                line_end_excluding_whitespace--;
            }
            line_metrics_.emplace_back(line_start, line_end, line_end_excluding_whitespace,
                                       line_end_including_newline, hard_break);
            line_widths_.emplace_back(breaker_.getWidths()[i]);
        }
        breaker_.finish();
    }

    return true;
}

bool ParagraphTxt::ComputeBidiRuns(std::vector<BidiRun>& result) {
    if (text_.empty()) {
        return true;
    }

    auto ubidi_closer = [](UBiDi* b) { ubidi_close(b); };
    std::unique_ptr<UBiDi, decltype(ubidi_closer)> bidi{ubidi_open(), ubidi_closer};

    if (!bidi) {
        return false;
    }

    UBiDiLevel paraLevel =
            (paragraph_style_.text_direction == TextDirection::kRtl) ? UBIDI_RTL : UBIDI_LTR;
    UErrorCode status = U_ZERO_ERROR;
    ubidi_setPara(bidi.get(), reinterpret_cast<const UChar*>(text_.data()), text_.size(), paraLevel,
                  nullptr, std::addressof(status));
    if (!U_SUCCESS(status)) {
        return false;
    }

    int32_t bidi_run_count = ubidi_countRuns(bidi.get(), std::addressof(status));
    if (!U_SUCCESS(status)) {
        return false;
    }

    // Detect if final trailing run is a single ambiguous whitespace.
    // We want to bundle the final ambiguous whitespace with the preceding
    // run in order to maintain logical typing behavior when mixing RTL and LTR
    // text. We do not want this to be a true ghost run since the contrasting
    // directionality causes the trailing space to not render at the visual end of
    // the paragraph.
    //
    // This only applies to the final whitespace at the end as other whitespace is
    // no longer ambiguous when surrounded by additional text.
    bool has_trailing_whitespace = false;
    int32_t bidi_run_start, bidi_run_length;
    if (bidi_run_count > 1) {
        ubidi_getVisualRun(bidi.get(), bidi_run_count - 1, std::addressof(bidi_run_start),
                           std::addressof(bidi_run_length));
        if (!U_SUCCESS(status)) {
            return false;
        }
        if (bidi_run_length == 1) {
            UChar32 last_char;
            U16_GET(text_.data(), 0, bidi_run_start + bidi_run_length - 1,
                    static_cast<int>(text_.size()), last_char);
            if (u_hasBinaryProperty(last_char, UCHAR_WHITE_SPACE)) {
                // Check if the trailing whitespace occurs before the previous run or
                // not. If so, this trailing whitespace was a leading whitespace.
                int32_t second_last_bidi_run_start, second_last_bidi_run_length;
                ubidi_getVisualRun(bidi.get(), bidi_run_count - 2,
                                   std::addressof(second_last_bidi_run_start),
                                   std::addressof(second_last_bidi_run_length));
                if (bidi_run_start == second_last_bidi_run_start + second_last_bidi_run_length) {
                    has_trailing_whitespace = true;
                    bidi_run_count--;
                }
            }
        }
    }

    // Build a map of styled runs indexed by start position.
    std::map<size_t, StyledRuns::Run> styled_run_map;
    for (size_t i = 0; i < runs_.Size(); i++) {
        StyledRuns::Run run = runs_.GetRun(i);
        styled_run_map.insert(std::make_pair(run.start, run));
    }

    for (int32_t bidi_run_index = 0; bidi_run_index < bidi_run_count; bidi_run_index++) {
        UBiDiDirection direction =
                ubidi_getVisualRun(bidi.get(), bidi_run_index, std::addressof(bidi_run_start),
                                   std::addressof(bidi_run_length));
        if (!U_SUCCESS(status)) {
            return false;
        }

        // Exclude the leading bidi control character if present.
        UChar32 first_char;
        U16_GET(text_.data(), 0, bidi_run_start, static_cast<int>(text_.size()), first_char);
        if (u_hasBinaryProperty(first_char, UCHAR_BIDI_CONTROL)) {
            bidi_run_start++;
            bidi_run_length--;
        }

        if (bidi_run_length == 0) {
            continue;
        }

        // Exclude the trailing bidi control character if present.
        UChar32 last_char;
        U16_GET(text_.data(), 0, bidi_run_start + bidi_run_length - 1,
                static_cast<int>(text_.size()), last_char);
        if (u_hasBinaryProperty(last_char, UCHAR_BIDI_CONTROL)) {
            bidi_run_length--;
        }
        if (bidi_run_length == 0) {
            continue;
        }

        // Attach the final trailing whitespace as part of this run.
        if (has_trailing_whitespace && bidi_run_index == bidi_run_count - 1) {
            bidi_run_length++;
        }

        size_t bidi_run_end = bidi_run_start + bidi_run_length;
        TextDirection text_direction =
                direction == UBIDI_RTL ? TextDirection::kRtl : TextDirection::kLtr;

        // Break this bidi run into chunks based on text style.
        std::vector<BidiRun> chunks;
        size_t chunk_start = bidi_run_start;
        while (chunk_start < bidi_run_end) {
            auto styled_run_iter = styled_run_map.upper_bound(chunk_start);
            styled_run_iter--;
            const StyledRuns::Run& styled_run = styled_run_iter->second;
            size_t chunk_end = std::min(bidi_run_end, styled_run.end);
            chunks.emplace_back(chunk_start, chunk_end, text_direction, styled_run.style);
            chunk_start = chunk_end;
        }

        if (text_direction == TextDirection::kLtr) {
            result.insert(result.end(), chunks.begin(), chunks.end());
        } else {
            result.insert(result.end(), chunks.rbegin(), chunks.rend());
        }
    }

    return true;
}

} // namespace txt
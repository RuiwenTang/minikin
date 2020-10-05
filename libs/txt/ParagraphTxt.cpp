//
// Created by TangRuiwen on 2020/10/02.
//

#include "ParagraphTxt.h"

#include <hb.h>
#include <minikin/Layout.h>
#include <unicode/ubidi.h>

#include <algorithm>
#include <cstring>
#include <numeric>

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

// Implementation outline:
//
// -For each line:
//   -Compute Bidi runs, convert into line_runs (keeps in-line-range runs, adds
//   special runs)
//   -For each line_run (runs in the line):
//     -Calculate ellipsis
//     -Obtain font
//     -layout.doLayout(...), genereates glyph blobs
//     -For each glyph blob:
//       -Convert glyph blobs into pixel metrics/advances
//     -Store as paint records (for painting) and code unit runs (for metrics
//     and boxes).

//   -Apply letter spacing, alignment, justification, etc
//   -Calculate line vertical layout (ascent, descent, etc)
//   -Store per-line metrics
void ParagraphTxt::Layout(double width) {
    double rounded_width = std::floor(width);
    // Do not allow calling layout multiple times without changing anything.
    if (!needs_layout_ && rounded_width == width) {
        return;
    }

    width_ = rounded_width;

    needs_layout_ = false;

    // records_.clear();
    glyph_lines_.clear();
    code_unit_runs_.clear();
    inline_placeholder_code_unit_runs_.clear();
    max_right_ = FLT_MIN;
    min_left_ = FLT_MAX;
    final_line_count_ = 0;

    if (!ComputeLineBreaks()) {
        return;
    }

    std::vector<BidiRun> bidi_runs;
    if (!ComputeBidiRuns(bidi_runs)) {
        return;
    }

    minikin::Layout layout;
    double y_offset = 0;
    double prev_max_descent = 0;
    double max_word_width = 0;

    // Compute strut minimums according to paragraph_style_.
    ComputeStruct(std::addressof(strut_), nullptr);

    // Paragraph bounds tracking.
    size_t line_limit = std::min(paragraph_style_.max_lines, line_metrics_.size());
    did_exceed_max_lines_ = (line_metrics_.size() > paragraph_style_.max_lines);

    size_t placeholder_run_index = 0;
    for (size_t line_number = 0; line_number < line_limit; line_number++) {
        LineMetrics& line_metrics = line_metrics_[line_number];

        // Break the line into words if justification should be applied.
        std::vector<Range<size_t>> words;
        double word_gap_width = 0;
        size_t word_index = 0;
        bool justify_line = (paragraph_style_.text_align == TextAlign::kJustify &&
                             line_number != line_limit - 1 && !line_metrics.hard_break);
        FindWords(text_, line_metrics.start_index, line_metrics.end_index, words);
        if (justify_line) {
            if (words.size() > 1) {
                word_gap_width = (width_ - line_widths_[line_number]) / (words.size() - 1);
            }
        }

        // Exclude trailing whitespace from justified lines so the last visible character in the
        // line will be flush with the right margin.
        size_t line_end_index = (paragraph_style_.effective_align() == TextAlign::kRight ||
                                 paragraph_style_.effective_align() == TextAlign::kCenter ||
                                 paragraph_style_.effective_align() == TextAlign::kJustify)
                ? line_metrics.end_excluding_whitespace
                : line_metrics.end_index;

        // Find the runs comprising this line.
        std::vector<BidiRun> line_runs;
        for (const BidiRun& bidi_run : bidi_runs) {
            // A "ghost" run is a run that does not impact the layout, breaking,
            // alignment, width, etc but is still "visible" through getRectsForRange.
            // For example, trailing whitespace on centered text can be scrolled
            // through with the caret but will not wrap the line.
            //
            // Here, we add an additional run for the whitespace, but dont
            // let it impact metrics. After layout of the whitespace run, we do not
            // add its width into the x-offset adjustment, effectively nullifying its
            // impact on the layout.
            std::unique_ptr<BidiRun> ghost_run = nullptr;
            if (paragraph_style_.ellipsis.empty() &&
                line_metrics.end_excluding_whitespace < line_metrics.end_index &&
                bidi_run.Start() <= line_metrics.end_index && bidi_run.End() > line_end_index) {
                ghost_run =
                        std::make_unique<BidiRun>(std::max(bidi_run.Start(), line_end_index),
                                                  std::min(bidi_run.End(), line_metrics.end_index),
                                                  bidi_run.Direction(), bidi_run.Style(), true);
            }
            // Include the ghost run before normal run if RTL
            if (bidi_run.Direction() == TextDirection::kRtl && ghost_run != nullptr) {
                line_runs.emplace_back(*ghost_run);
            }
            // Emplace a normal line run.
            if (bidi_run.Start() < line_end_index && bidi_run.End() > line_metrics.start_index) {
                // The run is a placeholder run.
                if (bidi_run.Size() == 1 && text_[bidi_run.Start()] == objReplacementChar &&
                    obj_replacement_char_indexes_.count(bidi_run.Start()) != 0 &&
                    placeholder_run_index < inline_placeholders_.size()) {
                    line_runs.emplace_back(std::max(bidi_run.Start(), line_metrics.start_index),
                                           std::min(bidi_run.End(), line_end_index),
                                           bidi_run.Direction(), bidi_run.Style(),
                                           inline_placeholders_[placeholder_run_index]);
                    placeholder_run_index++;
                } else {
                    line_runs.emplace_back(std::max(bidi_run.Start(), line_metrics.start_index),
                                           std::min(bidi_run.End(), line_end_index),
                                           bidi_run.Direction(), bidi_run.Style());
                }
            }

            // Include the ghost run after normal run if LTR
            if (bidi_run.Direction() == TextDirection::kLtr && ghost_run != nullptr) {
                line_runs.emplace_back(*ghost_run);
            }
        }
        bool line_runs_all_rtl = line_runs.size() &&
                std::accumulate(line_runs.begin(), line_runs.end(), true,
                                [](const bool a, const BidiRun& b) { return a && b.IsRTL(); });
        if (line_runs_all_rtl) {
            std::reverse(words.begin(), words.end());
        }

        std::vector<GlyphPosition> line_glyph_positions;
        std::vector<CodeUnitRun> line_code_unit_runs;
        std::vector<CodeUnitRun> line_inline_placeholder_code_unit_runs;

        double run_x_offset = 0;
        double justify_x_offset = 0;
        size_t cluster_unique_id = 0;
        // std::vector<PaintRecord> paint_records;

        for (auto line_run_it = line_runs.begin(); line_run_it != line_runs.end(); line_run_it++) {
            const BidiRun& run = *line_run_it;
            minikin::FontStyle minikin_font;
            minikin::MinikinPaint minikin_paint;
            GetFontAndMinikinPaint(run.Style(), std::addressof(minikin_font),
                                   std::addressof(minikin_paint));

            std::shared_ptr<minikin::FontCollection> minikin_font_collection =
                    GetMinikinFontCollectionForStyle(run.Style());

            // Layout this run.
            uint16_t* text_ptr = text_.data();
            size_t text_start = run.Start();
            size_t text_count = run.End() - run.Start();
            size_t text_size = text_.size();

            // Apply ellipsizing if the run was not completely laid out and this is the last line
            // (or lines are unlimited).
            const std::u16string& ellipsis = paragraph_style_.ellipsis;
            std::vector<uint16_t> ellipsized_text;
            if (ellipsis.length() && !std::isinf(width_) && !line_metrics.hard_break &&
                line_run_it == line_runs.end() - 1 &&
                (line_number == line_limit - 1 || paragraph_style_.unlimited_lines())) {
                float ellipsis_width =
                        layout.measureText(reinterpret_cast<const uint16_t*>(ellipsis.data()), 0,
                                           ellipsis.length(), ellipsis.length(), run.IsRTL(),
                                           minikin_font, minikin_paint,
                                           minikin_font_collection.get(), nullptr);

                std::vector<float> text_advances(text_count);
                float text_width =
                        layout.measureText(text_ptr, text_start, text_count, text_.size(),
                                           run.IsRTL(), minikin_font, minikin_paint,
                                           minikin_font_collection.get(), text_advances.data());

                // Truncate characters from the text until the ellpsis fits.
                size_t truncate_count = 0;
                while (truncate_count < text_count &&
                       run_x_offset + text_width + ellipsis_width > width_) {
                    text_width -= text_advances[text_count - truncate_count - 1];
                    truncate_count++;
                }

                ellipsized_text.reserve(text_count - truncate_count + ellipsis.length());
                ellipsized_text.insert(ellipsized_text.begin(), text_.begin() + run.Start(),
                                       text_.begin() + run.End() - truncate_count);
                ellipsized_text.insert(ellipsized_text.end(), ellipsis.begin(), ellipsis.end());

                text_ptr = ellipsized_text.data();
                text_start = 0;
                text_count = ellipsized_text.size();
                text_size = text_count;

                // If there is no line limit, then skip all lines after the ellipsized line.
                if (paragraph_style_.unlimited_lines()) {
                    line_limit = line_number + 1;
                    did_exceed_max_lines_ = true;
                }
            }

            layout.doLayout(text_ptr, text_start, text_count, text_size, run.IsRTL(), minikin_font,
                            minikin_paint, minikin_font_collection);

            if (layout.nGlyphs() == 0) {
                continue;
            }

            // When laying out RTL ghost runs, shift the run_x_offset here by the
            // advance so that the ghost run is positioned to the left of the first
            // real run of text in the line. However, since we do not want it to
            // impact the layout of real text, this advance is subsequently added
            // back into the run_x_offset after the ghost run positions have been
            // calcuated and before the next real run of text is laid out, ensuring
            // later runs are laid out in the same position as if there were no ghost
            // run.
            if (run.IsGhost() && run.IsRTL()) {
                run_x_offset -= layout.getAdvance();
            }

            std::vector<float> layout_advances(text_count);
            layout.getAdvances(layout_advances.data());

            // Break the layout into blobs that share the same Paint parameters.
            std::vector<Range<size_t>> glyph_blobs = GetLayoutTypefaceRuns(layout);

            double constexpr word_start_position = std::numeric_limits<double>::quiet_NaN();

            // TODO implement
            // Build  Text blob from each group of glyphs.
            for (const Range<size_t>& glyph_blob : glyph_blobs) {
                std::vector<GlyphPosition> glyph_positions;

                GetGlyphTypeface(layout, glyph_blob.start);
            }

            // Adjust the glyph positions based on the alignment of the line.
            double line_x_offset = GetLineXOffset(run_x_offset, justify_line);
            if (line_x_offset) {
                for (CodeUnitRun& code_unit_run : line_code_unit_runs) {
                    code_unit_run.Shift(line_x_offset);
                }
            }

            size_t next_line_start = (line_number < line_metrics_.size() - 1)
                    ? line_metrics_[line_number + 1].start_index
                    : text_.size();
            glyph_lines_.emplace_back(std::move(line_glyph_positions),
                                      next_line_start - line_metrics.start_index);
            code_unit_runs_.insert(code_unit_runs_.end(), line_code_unit_runs.begin(),
                                   line_code_unit_runs.end());
            inline_placeholder_code_unit_runs_
                    .insert(inline_placeholder_code_unit_runs_.end(),
                            line_inline_placeholder_code_unit_runs.begin(),
                            line_inline_placeholder_code_unit_runs.end());

            // Calculate the amount to advance in the y direction. This is done by
            // computing the maximum ascent and descent with respect to the strut.
            double max_ascent = strut_.ascent + strut_.half_leading;
            double max_descent = strut_.descent + strut_.half_leading;
            double max_unscaled_ascent = 0;

            // TODO UpdateLineMetrics

            // If no fonts were actually rendered, then compute a baseline based on the font of
            // paragraph style
            // TODO implement

            // Calcluate the baselines. This is only done on the first line.
            if (line_number == 0) {
                alphabetic_baseline_ = max_ascent;

                ideographic_baseline_ = (max_ascent + max_descent);
            }

            line_metrics.height = (line_number == 0 ? 0
                                                    : line_metrics_[line_number - 1].height +
                                                   std::round(max_ascent + max_descent));

            y_offset += std::round(max_ascent + prev_max_descent);
            prev_max_descent = max_descent;

            line_metrics.line_number = line_number;
            line_metrics.ascent = max_ascent;
            line_metrics.descent = max_descent;
            line_metrics.unscaled_ascent = max_unscaled_ascent;
            line_metrics.width = line_widths_[line_number];
            line_metrics.left = line_x_offset;

            final_line_count_++;
        }
    } // for each line_number

    if (paragraph_style_.max_lines == 1 ||
        (paragraph_style_.unlimited_lines() && paragraph_style_.ellipsized())) {
        min_intrinsic_width_ = max_intrinsic_width_;
    } else {
        min_intrinsic_width_ = std::min(max_word_width, max_intrinsic_width_);
    }

    std::sort(code_unit_runs_.begin(), code_unit_runs_.end(),
              [](const CodeUnitRun& a, const CodeUnitRun& b) {
                  return a.code_units.start < b.code_units.start;
              });

    longest_line_ = max_right_ - min_left_;
}

const ParagraphStyle& ParagraphTxt::GetParagraphStyle() const {
    return paragraph_style_;
}

size_t ParagraphTxt::TextSize() const {
    return text_.size();
}

double ParagraphTxt::GetHeight() {
    return final_line_count_ == 0 ? 0 : line_metrics_[final_line_count_ - 1].height;
}

double ParagraphTxt::GetMaxWidth() {
    return width_;
}

double ParagraphTxt::GetLongestLine() {
    return longest_line_;
}

void ParagraphTxt::SetText(std::vector<uint16_t> text, StyledRuns runs) {
    this->SetDirty(true);
    if (text.empty()) {
        return;
    }

    this->text_ = std::move(text);
    this->runs_ = std::move(runs);
}

void ParagraphTxt::SetParagraphStyle(const ParagraphStyle& style) {
    needs_layout_ = true;
    paragraph_style_ = style;
}

void ParagraphTxt::SetFontCollection(std::shared_ptr<FontCollection> font_collection) {
    font_collection_ = std::move(font_collection);
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

void ParagraphTxt::ComputeStruct(StrutMetrics* strut, minikin::MinikinFont* /*font*/) {
    strut->ascent = 0;
    strut->descent = 0;
    strut->leading = 0;
    strut->half_leading = 0;
    strut->line_height = 0;
    strut->force_strut = false;

    if (!IsStrutValid()) {
        return;
    }

    // force_strut makes all lines have exactly the strut metrics, and ignores all actual metrics.
    // we only force the strut if the strut is non-zero and valid.
    strut->force_strut = paragraph_style_.force_strut_height;
    minikin::FontStyle minikin_font_style{0, GetWeight(paragraph_style_.strut_font_weight),
                                          paragraph_style_.strut_font_style == FontStyle::Italic};

    std::shared_ptr<minikin::FontCollection> collection =
            font_collection_
                    ->GetMinikinFontCollectionForFamilies(paragraph_style_.strut_font_families, "");
    if (!collection) {
        return;
    }

    minikin::FakedFont faked_font = collection->baseFontFaked(minikin_font_style);

    if (faked_font.font != nullptr) {
        std::string family_name = faked_font.font->GetFamilyName();
        // TODO make a font adapter

        if (paragraph_style_.strut_has_height_override) {
        }

        strut->half_leading = strut->leading / 2.0;
        strut->line_height = strut->ascent + strut->descent + strut->leading;
    }
}

void ParagraphTxt::ComputePlaceholder(PlaceholderRun* placeholder_run, double& ascent,
                                      double& descent) {
    if (placeholder_run != nullptr) {
        // Calculate how much to shift the ascent and descent to account for the baseline choice.

        // Currently only supports for alphabetic and ideographic
        double baseline_adjustment = 0;
        switch (placeholder_run->baseline) {
            case TextBaseline::kAlphabetic: {
                baseline_adjustment = 0;
                break;
            }
            case TextBaseline::kIdeographic: {
                baseline_adjustment = -descent / 2;
                break;
            }
        }

        // Convert the ascent and descent from the font's to the placeholder rect's
        switch (placeholder_run->alignment) {
            case PlaceholderAlignment::kBaseline: {
                ascent = baseline_adjustment + placeholder_run->baseline_offset;
                descent = -baseline_adjustment + placeholder_run->height -
                        placeholder_run->baseline_offset;
                break;
            }
            case PlaceholderAlignment::kAboveBaseline: {
                ascent = baseline_adjustment + placeholder_run->height;
                descent = -baseline_adjustment;
                break;
            }
            case PlaceholderAlignment::kBelowBaseline: {
                descent = baseline_adjustment + placeholder_run->height;
                ascent = -baseline_adjustment;
                break;
            }
            case PlaceholderAlignment::kTop: {
                descent = placeholder_run->height - descent;
                break;
            }
            case PlaceholderAlignment::kBottom: {
                ascent = placeholder_run->height - descent;
                break;
            }
            case PlaceholderAlignment::kMiddle: {
                double mid = (ascent - descent) / 2;
                ascent = mid + placeholder_run->height / 2;
                descent = -mid + placeholder_run->height / 2;
                break;
            }
        }
        placeholder_run->baseline_offset = ascent;
    }
}

bool ParagraphTxt::IsStrutValid() const {
    // Font size must be positive.
    return (paragraph_style_.strut_enabled && paragraph_style_.strut_font_size >= 0);
}

double ParagraphTxt::GetLineXOffset(double line_total_advance, bool justify_line) {
    if (std::isinf(width_)) {
        return 0;
    }

    TextAlign align = paragraph_style_.effective_align();

    if (align == TextAlign::kRight ||
        (align == TextAlign::kJustify && paragraph_style_.text_direction == TextDirection::kRtl &&
         !justify_line)) {
        return width_ - line_total_advance;
    } else if (align == TextAlign::kCenter) {
        return (width_ - line_total_advance) / 2.0;
    } else {
        return 0.0;
    }
}

std::shared_ptr<minikin::FontCollection> ParagraphTxt::GetMinikinFontCollectionForStyle(
        const TextStyle& style) {
    std::string locale;
    if (!style.local.empty()) {
        uint32_t language_list_id = minikin::FontStyle::registerLanguageList(style.local);
        const minikin::FontLanguages& langs =
                minikin::FontLanguageListCache::getById(language_list_id);
        if (langs.size()) {
            locale = langs[0].getString();
        }
    }
    return font_collection_->GetMinikinFontCollectionForFamilies(style.font_families, locale);
}

double ParagraphTxt::GetAlphabeticBaseline() {
    return alphabetic_baseline_;
}

double ParagraphTxt::GetIdeographicBaseline() {
    return ideographic_baseline_;
}

double ParagraphTxt::GetMaxIntrinsicWidth() {
    return max_intrinsic_width_;
}

double ParagraphTxt::GetMinIntrinsicWidth() {
    return min_intrinsic_width_;
}

// struct that holds calculated metrics for each line.
static struct LineBoxMetrics {
    std::vector<Paragraph::TextBox> boxes;
    float max_right = FLT_MIN;
    float min_left = FLT_MAX;
};

std::vector<Paragraph::TextBox> ParagraphTxt::GetRectsForRange(size_t start, size_t end,
                                                               RectHeightStyle rect_height_style,
                                                               RectWidthStyle rect_width_style) {
    std::map<size_t, LineBoxMetrics> line_box_metrics;
    // Text direction of the first line so we can extend the correct side for
    // RectWidthStyle::kMax.
    TextDirection first_line_dir = TextDirection::kLtr;
    std::map<size_t, size_t> newline_x_positions;

    // Lines that are actually in the requested range.
    size_t max_line = 0;
    size_t min_line = INT_MAX;
    size_t glyph_length = 0;

    // Generate initial boxes and calculate metrics.
    for (const CodeUnitRun& run : code_unit_runs_) {
        // Check to see if we are finished.
        if (run.code_units.start >= end) {
            break;
        }

        // Update new line x position with the ending of last bidi run on the line
        newline_x_positions[run.line_number] =
                run.direction == TextDirection::kLtr ? run.x_pos.end : run.x_pos.start;

        if (run.code_units.end <= start) {
            continue;
        }

        double baseline = line_metrics_[run.line_number].baseline;
        // TODO implement run.font_metrics
        float top = 0;
        float bottom = 0;

        if (run.placeholder_run != nullptr) {
            // Use inline placeholder size as height.
            top = baseline - run.placeholder_run->baseline_offset;
            bottom = baseline + run.placeholder_run->height - run.placeholder_run->baseline_offset;
        }

        max_line = std::max(run.line_number, max_line);
        min_line = std::min(run.line_number, min_line);

        // Calculate left and right.
        float left, right;
        if (run.code_units.start >= start && run.code_units.end <= end) {
            left = run.x_pos.start;
            right = run.x_pos.end;
        } else {
            left = std::numeric_limits<float>::max();
            right = std::numeric_limits<float>::min();
            for (const GlyphPosition& gp : run.positions) {
                if (gp.code_units.start >= start && gp.code_units.end <= end) {
                    left = std::min(left, static_cast<float>(gp.x_pos.start));
                    right = std::max(right, static_cast<float>(gp.x_pos.end));
                } else if (gp.code_units.end == end) {
                    // Calculate left and right when we are at
                    // the last position of a combining character.
                    glyph_length = (gp.code_units.end - gp.code_units.start) - 1;
                    if (gp.code_units.start == std::max<size_t>(0, (start - glyph_length))) {
                        left = std::min(left, static_cast<float>(gp.x_pos.start));
                        right = std::max(right, static_cast<float>(gp.x_pos.end));
                    }
                }
            }

            if (left == std::numeric_limits<float>::max() ||
                right == std::numeric_limits<float>::min()) {
                continue;
            }
        }

        // Keep track of the min and max horizontal coordinates over all lines. Not
        // needed for kTight.
        if (rect_width_style == RectWidthStyle::kMax) {
            line_box_metrics[run.line_number].max_right =
                    std::max(line_box_metrics[run.line_number].max_right, right);
            line_box_metrics[run.line_number].min_left =
                    std::min(line_box_metrics[run.line_number].min_left, left);
            if (min_line == run.line_number) {
                first_line_dir = run.direction;
            }
        }

        line_box_metrics[run.line_number].boxes.emplace_back(minikin::MinikinRect{left, top, right,
                                                                                  bottom},
                                                             run.direction);
    }

    // Add empty rectangles representing any newline characters within the
    // range.
    for (size_t line_number = 0; line_number < line_metrics_.size(); line_number++) {
        LineMetrics& line = line_metrics_[line_number];
        if (line.start_index >= end) {
            break;
        }
        if (line.end_including_newline <= start) {
            continue;
        }

        if (line_box_metrics.find(line_number) != line_box_metrics.end()) {
            if (line.end_index != line.end_including_newline && line.end_index >= start &&
                line.end_including_newline <= end) {
                float x;
                auto it = newline_x_positions.find(line_number);
                if (it != newline_x_positions.end()) {
                    x = it->second;
                } else {
                    x = GetLineXOffset(0, false);
                }

                float top = (line_number > 0) ? line_metrics_[line_number - 1].height : 0;
                float bottom = line_metrics_[line_number].height;
                line_box_metrics[line_number].boxes.emplace_back(minikin::MinikinRect{x, top, x,
                                                                                      bottom},
                                                                 TextDirection::kLtr);
            }
        }
    }

    // "Post-process" metrics and aggregate final rects to return.
    std::vector<Paragraph::TextBox> boxes;
    for (const auto& kv : line_box_metrics) {
        // Handle rect_width_styles. We skip the last line because not everything is
        // selected.

        LineMetrics& line =
                line_metrics_[std::fmin(line_metrics_.size() - 1, std::fmax(0, kv.first))];
        if (rect_width_style == RectWidthStyle::kMax && kv.first != max_line) {
            if (line_box_metrics[kv.first].min_left > min_left_ &&
                (kv.first != min_line || first_line_dir == TextDirection::kRtl)) {
                line_box_metrics[kv.first]
                        .boxes
                        .emplace_back(minikin::MinikinRect{static_cast<float>(min_left_),
                                                           static_cast<float>(line.baseline -
                                                                              line.unscaled_ascent),
                                                           line_box_metrics[kv.first].min_left,
                                                           static_cast<float>(line.baseline +
                                                                              line.descent)},
                                      TextDirection::kRtl);
            }
            if (line_box_metrics[kv.first].max_right < max_right_ &&
                (kv.first != min_line || first_line_dir == TextDirection::kLtr)) {
                line_box_metrics[kv.first]
                        .boxes
                        .emplace_back(minikin::MinikinRect{line_box_metrics[kv.first].max_right,
                                                           static_cast<float>(line.baseline -
                                                                              line.unscaled_ascent),
                                                           static_cast<float>(max_right_),
                                                           static_cast<float>(line.baseline +
                                                                              line.descent)},
                                      TextDirection::kLtr);
            }
        }

        // Handle rect_height_styles. The height metrics used are all positive to
        // make the signage clear here.
        if (rect_height_style == RectHeightStyle::kTight) {
            // Ignore line max height and width and generate tight bounds.
            boxes.insert(boxes.end(), kv.second.boxes.begin(), kv.second.boxes.end());
        } else if (rect_height_style == RectHeightStyle::kMax) {
            for (const Paragraph::TextBox& box : kv.second.boxes) {
                boxes.emplace_back(minikin::MinikinRect{box.rect.mLeft,
                                                        static_cast<float>(line.baseline -
                                                                           line.ascent),
                                                        box.rect.mRight,
                                                        static_cast<float>(line.baseline +
                                                                           line.descent)},
                                   box.direction);
            }
        } else if (rect_height_style == RectHeightStyle::kIncludeLineSpacingMiddle) {
            float adjusted_bottom = line.baseline + line.descent;
            if (kv.first < line_metrics_.size() - 1) {
                adjusted_bottom += (line_metrics_[kv.first + 1].ascent -
                                    line_metrics_[kv.first + 1].unscaled_ascent / 2);
            }

            float adjusted_top = line.baseline - line.unscaled_ascent;
            if (kv.first != 0) {
                adjusted_top -= (line.ascent - line.unscaled_ascent) / 2.0;
            }
            for (const Paragraph::TextBox& box : kv.second.boxes) {
                boxes.emplace_back(minikin::MinikinRect{box.rect.mLeft, adjusted_top,
                                                        box.rect.mRight, adjusted_bottom},
                                   box.direction);
            }
        } else if (rect_height_style == RectHeightStyle::kIncludeLineSpacingTop) {
            for (const Paragraph::TextBox& box : kv.second.boxes) {
                float adjusted_top = kv.first == 0 ? line.baseline - line.unscaled_ascent
                                                   : line.baseline - line.ascent;
                boxes.emplace_back(minikin::MinikinRect{box.rect.mLeft, adjusted_top,
                                                        box.rect.mRight,
                                                        static_cast<float>(line.baseline +
                                                                           line.descent)},
                                   box.direction);
            }
        } else if (rect_height_style == RectHeightStyle::kStrut) {
            if (IsStrutValid()) {
                for (const Paragraph::TextBox& box : kv.second.boxes) {
                    boxes.emplace_back(minikin::MinikinRect{box.rect.mLeft,
                                                            static_cast<float>(line.baseline -
                                                                               strut_.ascent),
                                                            box.rect.mRight,
                                                            static_cast<float>(line.baseline +
                                                                               strut_.descent)},
                                       box.direction);
                }
            } else {
                // Fall back to tight bounds if the strut is invalid.
                boxes.insert(boxes.end(), kv.second.boxes.begin(), kv.second.boxes.end());
            }
        }
    }

    return boxes;
}

Paragraph::PositionWithAffinity ParagraphTxt::GetGlyphPositionAtCoordinate(double dx, double dy) {
    if (final_line_count_ <= 0) {
        return {0, DOWNSTREAM};
    }

    size_t y_index;
    for (y_index = 0; y_index < final_line_count_ - 1; ++y_index) {
        if (dy < line_metrics_[y_index].height) break;
    }

    const std::vector<GlyphPosition>& line_glyph_position = glyph_lines_[y_index].positions;
    if (line_glyph_position.empty()) {
        int line_start_index = std::accumulate(glyph_lines_.begin(), glyph_lines_.begin() + y_index,
                                               0, [](const int a, const GlyphLine& b) {
                                                   return a + static_cast<int>(b.total_code_units);
                                               });
        return {static_cast<size_t>(line_start_index), DOWNSTREAM};
    }

    size_t x_index;
    const GlyphPosition* gp = nullptr;
    const GlyphPosition* gp_cluster = nullptr;
    bool is_cluster_corection = false;
    for (x_index = 0; x_index < line_glyph_position.size(); ++x_index) {
        double glyph_end = (x_index < line_glyph_position.size() - 1)
                ? line_glyph_position[x_index + 1].x_pos.start
                : line_glyph_position[x_index].x_pos.end;
        if (gp_cluster == nullptr || gp_cluster->cluster != line_glyph_position[x_index].cluster) {
            gp_cluster = &line_glyph_position[x_index];
        }
        if (dx < glyph_end) {
            // Check if the glyph position is part of a cluster. If it is,
            // we assign the cluster's root GlyphPosition to represent it.
            if (gp_cluster->cluster == line_glyph_position[x_index].cluster) {
                gp = gp_cluster;
                // Detect if the matching GlyphPosition was non-root for the cluster.
                if (gp_cluster != &line_glyph_position[x_index]) {
                    is_cluster_corection = true;
                }
            } else {
                gp = &line_glyph_position[x_index];
            }
            break;
        }
    }

    if (gp == nullptr) {
        const GlyphPosition& last_glyph = line_glyph_position.back();
        return {last_glyph.code_units.end, UPSTREAM};
    }

    // Find the direction of the run that contains this glyph.
    TextDirection direction = TextDirection::kLtr;
    for (const CodeUnitRun& run : code_unit_runs_) {
        if (gp->code_units.start >= run.code_units.start &&
            gp->code_units.end <= run.code_units.end) {
            direction = run.direction;
            break;
        }
    }

    double glyph_center = (gp->x_pos.start + gp->x_pos.end) / 2;
    // We want to use the root cluster's start when the cluster
    // was corrected.
    // TODO(garyq): Detect if the position is in the middle of the cluster
    // and properly assign the start/end positions.
    if ((direction == TextDirection::kLtr && dx < glyph_center) ||
        (direction == TextDirection::kRtl && dx >= glyph_center) || is_cluster_corection) {
        return {gp->code_units.start, DOWNSTREAM};
    } else {
        return {gp->code_units.end, UPSTREAM};
    }
}

std::vector<Paragraph::TextBox> ParagraphTxt::GetRectsForPlaceholders() {
    std::vector<ParagraphTxt::TextBox> boxes;

    // Generate initial boxes and calculate metrics.
    for (const CodeUnitRun& run : inline_placeholder_code_unit_runs_) {
        // Check to see if we are finished.
        double baseline = line_metrics_[run.line_number].baseline;
        //        SkScalar top = baseline + run.font_metrics.fAscent;
        //        SkScalar bottom = baseline + run.font_metrics.fDescent;
        float top = 0.f;
        float bottom = 0.f;

        if (run.placeholder_run != nullptr) { // Use inline placeholder size as height.
            top = baseline - run.placeholder_run->baseline_offset;
            bottom = baseline + run.placeholder_run->height - run.placeholder_run->baseline_offset;
        }

        // Calculate left and right.
        float left, right;
        left = run.x_pos.start;
        right = run.x_pos.end;

        boxes.emplace_back(minikin::MinikinRect{left, top, right, bottom}, run.direction);
    }
    return boxes;
}

Paragraph::Range<size_t> ParagraphTxt::GetWordBoundary(size_t offset) {
    if (text_.empty()) {
        return Range<size_t>{0, 0};
    }

    if (!word_breaker_) {
        UErrorCode status = U_ZERO_ERROR;
        word_breaker_.reset(icu::BreakIterator::createWordInstance(icu::Locale(), status));
        if (!U_SUCCESS(status)) {
            return Range<size_t>{0, 0};
        }
    }

    word_breaker_->setText(icu::UnicodeString(false, text_.data(), text_.size()));

    int32_t prev_boundary = word_breaker_->preceding(offset + 1);
    int32_t next_boundary = word_breaker_->next();
    if (prev_boundary == icu::BreakIterator::DONE) prev_boundary = offset;
    if (next_boundary == icu::BreakIterator::DONE) next_boundary = offset;
    return Range<size_t>{static_cast<size_t>(prev_boundary), static_cast<size_t>(next_boundary)};
}

size_t ParagraphTxt::GetLineCount() {
    return final_line_count_;
}

bool ParagraphTxt::DidExceedMaxLines() {
    return did_exceed_max_lines_;
}

void ParagraphTxt::SetDirty(bool dirty) {
    needs_layout_ = dirty;
}

std::vector<LineMetrics>& ParagraphTxt::GetLineMetrics() {
    return line_metrics_;
}

} // namespace txt
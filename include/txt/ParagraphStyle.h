//
// Created by TangRuiwen on 2020/9/30.
//

#ifndef MINIKIN_INCLUDE_TXT_PARAGRAPHSTYLE_H_
#define MINIKIN_INCLUDE_TXT_PARAGRAPHSTYLE_H_

#include <minikin/LineBreaker.h>

#include <climits>
#include <map>
#include <string>
#include <vector>

namespace txt {

enum class FontStyle {
    Normal,
    Italic,
};

enum class FontWeight {
    w100, // Thin
    w200, // Extra-Light
    w300, // Light
    w400, // Normal/Regular
    w500, // Medium
    w600, // Semi-bold
    w700, // Bold
    w800, // Extra-Bold
    w900, // Black
};

enum class TextAlign {
    kLeft,
    kRight,
    kCenter,
    kJustify,
    kStart,
    kEnd,
};

enum class TextDirection {
    kRtl,
    kLtr,
};

// Allows disabling height adjustments to first line's ascent and the
// last line's descent. If disabled, the line will use the default font
// metric provided ascent/descent and ParagraphStyle.height will not take
// effect.
//
// The default behavior is kAll where height adjustments are enabled for all
// lines.
//
// Multiple behaviors can be applied at once with a bitwise | operator. For
// example, disabling first ascent and last descent can achieved with:
//
//   (kDisableFirstAscent | kDisableLastDescent).
enum TextHeightBehavior {
    kAll = 0x0,
    kDisableFirstAscent = 0x1,
    kDisableLastDescent = 0x2,
    kDisableAll = 0x1 | 0x2,
};

class TextStyle {
public:
    FontWeight font_weight = FontWeight::w400;
    FontStyle font_style = FontStyle::Normal;
    // An ordered list of fonts in order of priority. The first font is more
    // highly preferred than the last font.
    std::vector<std::string> font_families;

    double font_size = 14.0;
    double letter_spacing = 0.0;
    double word_spacing = 0.0;
    double height = 1.0;
    bool has_height_override = false;
    std::string local;

    TextStyle();
    ~TextStyle() = default;
};

class RunMetrics {
public:
    explicit RunMetrics(const TextStyle* text_style) : text_style(text_style) {}
    const TextStyle* text_style;
};

class LineMetrics {
public:
    size_t start_index = 0;
    size_t end_index = 0;
    size_t end_excluding_whitespace = 0;
    size_t end_including_newline = 0;
    bool hard_break = false;

    // The following fields are tracked after or during layout to provide to
    // the user as well as for computing bounding boxes.

    // The final computed ascent and descent for the line. This can be impacted by
    // the strut, height, scaling, as well as outlying runs that are very tall.
    //
    // The top edge is `baseline - ascent` and the bottom edge is `baseline +
    // descent`. Ascent and descent are provided as positive numbers. Raw numbers
    // for specific runs of text can be obtained in run_metrics_map. These values
    // are the cumulative metrics for the entire line.
    double ascent = 0.0;
    double descent = 0.0;
    double unscaled_ascent = 0.0;
    // Total height of the paragraph including the current line.
    //
    // The height of the current line is `round(ascent + descent)`.
    double height = 0.0;
    // Width of the line.
    double width = 0.0;
    // The left edge of the line. The right edge can be obtained with `left +
    // width`
    double left = 0.0;
    // The y position of the baseline for this line from the top of the paragraph.
    double baseline = 0.0;
    // Zero indexed line number.
    size_t line_number = 0;

    // Mapping between text index ranges and the FontMetrics associated with
    // them. The first run will be keyed under start_index. The metrics here
    // are before layout and are the base values we calculate from.
    std::map<size_t, RunMetrics> run_metrics;

    LineMetrics(size_t start, size_t end, size_t end_excluding_whitespace,
                size_t end_including_newline, bool hard_break)
          : start_index(start),
            end_index(end),
            end_excluding_whitespace(end_excluding_whitespace),
            end_including_newline(end_including_newline),
            hard_break(hard_break) {}
};

class ParagraphStyle final {
public:
    FontWeight font_weight = FontWeight::w400;
    FontStyle font_style = FontStyle::Normal;
    std::string font_family = "";
    double font_size = 14;
    double height = 1;
    size_t text_height_behavior = TextHeightBehavior::kAll;
    bool has_height_override = false;

    bool strut_enabled = false;
    FontWeight strut_font_weight = FontWeight::w400;
    FontStyle strut_font_style = FontStyle::Normal;
    std::vector<std::string> strut_font_families;
    double strut_font_size = 14;
    double strut_height = 1;
    bool strut_has_height_override = false;

    TextAlign text_align = TextAlign::kStart;
    TextDirection text_direction = TextDirection::kLtr;

    size_t max_lines = std::numeric_limits<size_t>::max();
    std::u16string ellipsis;
    std::string local;

    // Default strategy is kBreakStrategy_Greedy. Sometimes,
    // kBreakStrategy_HighQuality will produce more desirable layouts (e.g., very
    // long words are more likely to be reasonably placed).
    // kBreakStrategy_Balanced will balance between the two.
    android::BreakStrategy break_strategy = android::BreakStrategy::kBreakStrategy_Greedy;

    ParagraphStyle() = default;
    ~ParagraphStyle() = default;

    TextStyle GetTextStyle() const;

private:
    bool unlimited_lines() const;
    bool ellipsized() const;
    TextAlign effective_align() const;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_PARAGRAPHSTYLE_H_

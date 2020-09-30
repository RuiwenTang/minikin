//
// Created by TangRuiwen on 2020/9/30.
//

#ifndef MINIKIN_INCLUDE_TXT_PLACEHOLDERRUN_H_
#define MINIKIN_INCLUDE_TXT_PLACEHOLDERRUN_H_

namespace txt {

/// Where to vertically align the placeholder relative to the surrounding text.
enum class PlaceholderAlignment {
    /// Match the baseline of the placeholder with the baseline.
    kBaseline,

    /// Align the bottom edge of the placeholder with the baseline such that the
    /// placeholder sits on top of the baseline.
    kAboveBaseline,

    /// Align the top edge of the placeholder with the baseline specified in
    /// such that the placeholder hangs below the baseline.
    kBelowBaseline,

    /// Align the top edge of the placeholder with the top edge of the font.
    /// When the placeholder is very tall, the extra space will hang from
    /// the top and extend through the bottom of the line.
    kTop,

    /// Align the bottom edge of the placeholder with the top edge of the font.
    /// When the placeholder is very tall, the extra space will rise from
    /// the bottom and extend through the top of the line.
    kBottom,

    /// Align the middle of the placeholder with the middle of the text. When the
    /// placeholder is very tall, the extra space will grow equally from
    /// the top and bottom of the line.
    kMiddle,
};

// Represents the metrics required to fully define a rect that will fit a
// placeholder.
//
// LibTxt will leave an empty space in the layout of the text of the size
// defined by this class. After layout, the framework will draw placeholders
// into the reserved space.
class PlaceholderRun {
public:
    double width = 0;
    double height = 0;

    PlaceholderAlignment alignment = PlaceholderAlignment::kMiddle;

    // Distance from the top edge of the rect to the baseline position. This
    // baseline will be aligned against the alphabetic baseline of the surrounding
    // text.
    //
    // Positive values drop the baseline lower (positions the rect higher) and
    // small or negative values will cause the rect to be positioned underneath
    // the line. When baseline == height, the bottom edge of the rect will rest on
    // the alphabetic baseline.
    double baseline_offset = 0;

    PlaceholderRun() = default;

    PlaceholderRun(double width, double height, PlaceholderAlignment alignment,
                   double baseline_offset)
          : width{width}, height{height}, alignment{alignment}, baseline_offset{baseline_offset} {}
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_PLACEHOLDERRUN_H_

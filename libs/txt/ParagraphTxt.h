//
// Created by TangRuiwen on 2020/10/02.
//

#include <minikin/LineBreaker.h>
#include <txt/FontCollection.h>
#include <txt/Paragraph.h>
#include <txt/ParagraphStyle.h>
#include <txt/PlaceHolderRun.h>
#include <txt/StyledRuns.h>

#include <unordered_set>

namespace txt {

using GlyphID = uint32_t;

// Constant with the unicode codepoint for the "Object replacement character".
// Used as a stand-in character for Placeholder boxes.
const int objReplacementChar = 0xFFFC;
// Constant with the unicode codepoint for the "Replacement character". This is
// the character that commonly renders as a black diamond with a white question
// mark. Used to replace non-placeholder instances of 0xFFFC in the text buffer.
const int replacementChar = 0xFFFD;

// Paragraph provides Layout, metrics, and painting capabilities for text. Once
// a Paragraph is constructed with ParagraphBuilder::Build(), an example basic
// workflow can be this:
//
//   std::unique_ptr<Paragraph> paragraph = paragraph_builder.Build();
//   paragraph->Layout(<somewidthgoeshere>);
//   paragraph->Paint(<someSkCanvas>, <xpos>, <ypos>);
class ParagraphTxt : public Paragraph {
public:
    ParagraphTxt();
    ~ParagraphTxt() override;

    // Minikin Layout doLayout() and LineBreaker addStyleRun() has an
    // O(N^2) (according to benchmarks) time complexity where N is the total
    // number of characters. However, this is not significant for reasonably sized
    // paragraphs. It is currently recommended to break up very long paragraphs
    // (10k+ characters) to ensure speedy layout.KD
    void Layout(double width) override;

    // TODO implement paint interface
    // void Paint();

    const ParagraphStyle& GetParagraphStyle() const;

    size_t TextSize() const;

    double GetHeight() override;
    double GetMaxWidth() override;
    double GetLongestLine() override;
    double GetAlphabeticBaseline() override;
    double GetIdeographicBaseline() override;
    double GetMaxIntrinsicWidth() override;
    double GetMinIntrinsicWidth() override;
    std::vector<Paragraph::TextBox> GetRectsForRange(size_t start, size_t end,
                                                     RectHeightStyle rect_height_style,
                                                     RectWidthStyle rect_width_style) override;
    PositionWithAffinity GetGlyphPositionAtCoordinate(double dx, double dy) override;
    std::vector<Paragraph::TextBox> GetRectsForPlaceholders() override;
    Paragraph::Range<size_t> GetWordBoundary(size_t offset) override;

    // Returns the number of lines the paragraph takes up. If the text exceeds the
    // amount width and maxlines provides, Layout() truncates the extra text from
    // the layout and this will return the max lines allowed.
    size_t GetLineCount();

    bool DidExceedMaxLines() override;
    std::vector<LineMetrics>& GetLineMetrics() override;

    // Sets the needs_layout_ to dirty. When Layout() is called, a new Layout will
    // be performed when this is set to true. Can also be used to prevent a new
    // Layout from being calculated by setting to false.
    void SetDirty(bool dirty = true);

private:
    friend class ParagraphBuilderTxt;
    // Starting data to layout.
    std::vector<uint16_t> text_;
    // A vector of PlaceholderRuns, which detail the sizes, positioning and break
    // behavior of the empty spaces to leave. Each placeholder span corresponds to
    // a 0xFFFC (object replacement character) in text_, which indicates the
    // position in the text where the placeholder will occur. There should be an
    // equal number of 0xFFFC characters and elements in this vector.
    std::vector<PlaceholderRun> inline_placeholders_;
    // The indexes of the boxes that correspond to an inline placeholder.
    std::vector<size_t> inline_placeholder_boxes_;
    // The indexes of instances of 0xFFFC that correspond to placeholders. This is
    // necessary since the user may pass in manually entered 0xFFFC values using
    // AddText().
    std::unordered_set<size_t> obj_replacement_char_indexes_;
    StyledRuns runs_;
    ParagraphStyle paragraph_style_;
    std::shared_ptr<FontCollection> font_collection_;

    minikin::LineBreaker breaker_;
    mutable std::unique_ptr<icu::BreakIterator> word_breaker_;

    std::vector<LineMetrics> line_metrics_;
    size_t final_line_count_;
    std::vector<double> line_widths_;

    // Stores the result of Layout().
    // std::vector<PaintRecord> records_;

    bool did_exceed_max_lines_;

    // Strut metrics of zero will have no effect on the layout.
    struct StrutMetrics {
        double ascent = 0; // Positive value to keep signs clear.
        double descent = 0;
        double leading = 0;
        double half_leading = 0;
        double line_height = 0;
        bool force_strut = false;
    };
    StrutMetrics strut_;

    // Overall left and right extremes over all lines.
    double max_right_;
    double min_left_;

    class BidiRun {
    public:
        BidiRun(size_t start, size_t end, TextDirection directio, const TextStyle& style)
              : start_(start),
                end_(end),
                direction_(directio),
                style_(std::addressof(style)),
                is_ghost_(false) {}

        BidiRun(size_t start, size_t end, TextDirection direction, const TextStyle& style,
                PlaceholderRun& placeholder)
              : start_(start),
                end_(end),
                direction_(direction),
                style_(std::addressof(style)),
                is_ghost_(false),
                placeholder_run_(std::addressof(placeholder)) {}

        BidiRun(size_t start, size_t end, TextDirection direction, const TextStyle& style,
                bool is_ghost)
              : start_(start),
                end_(end),
                direction_(direction),
                style_(std::addressof(style)),
                is_ghost_(is_ghost),
                placeholder_run_(nullptr) {}

        size_t Start() const { return start_; }
        size_t End() const { return end_; }
        size_t Size() const { return end_ - start_; }
        TextDirection Direction() const { return direction_; }
        const TextStyle& Style() const { return *style_; }
        PlaceholderRun* PlaceholdrRun() const { return placeholder_run_; }
        bool IsRTL() const { return direction_ == TextDirection::kRtl; }
        // Tracks if the run represents trailing whitespace.
        bool IsGhost() const { return is_ghost_; }
        bool IsPlaceholderRun() const { return placeholder_run_ != nullptr; }

    private:
        size_t start_, end_;
        TextDirection direction_;
        const TextStyle* style_;
        bool is_ghost_;
        PlaceholderRun* placeholder_run_ = nullptr;
    };

    struct GlyphPosition {
        Range<size_t> code_units;
        Range<double> x_pos;
        // Tracks the cluster that this glyph position belongs to. For example, in
        // extended emojis, multiple glyph positions will have the same cluster. The
        // cluster can be used as a key to distinguish between codepoints that
        // contribute to the drawing of a single glyph.
        size_t cluster;

        GlyphPosition(double x_start, double x_advance, size_t code_unit_index,
                      size_t code_unit_width, size_t cluster);

        void Shift(double delta);
    };

    struct GlyphLine {
        // Glyph positions stred by x coordinate.
        const std::vector<GlyphPosition> positions;
        const size_t total_code_units;

        GlyphLine(std::vector<GlyphPosition>&& positions, size_t total_code_units);
    };

    struct CodeUnitRun {
        // Glyph positions stred by code unit index.
        std::vector<GlyphPosition> positions;
        Range<size_t> code_units;
        Range<double> x_pos;
        size_t line_number;
        // FontMetrics;
        // font_metrics;
        const TextStyle* style;
        TextDirection direction;
        const PlaceholderRun* placeholder_run;

        CodeUnitRun(std::vector<GlyphPosition>&& positions, Range<size_t> code_units,
                    Range<double> x_pos, size_t line, /* font_metrics, */ const TextStyle& style,
                    TextDirection direction, const PlaceholderRun* placeholder);

        void Shift(double delta);
    };

    // Holds the layout x posotions of each glyph.
    std::vector<GlyphLine> glyph_lines_;

    // Holds the positions of each range of code units in the text.
    // Sorted in code unit index order.
    std::vector<CodeUnitRun> code_unit_runs_;
    // Holds the positions of the inline placeholders.
    std::vector<CodeUnitRun> inline_placeholder_code_unit_runs_;

    // The max width of the paragraph as privided in the mos recent layout() call.
    double width_ = -1.0;
    double longest_line_ = -1.0;
    double max_intrinsic_width_ = 0;
    double min_intrinsic_width_ = 0;
    double alphabetic_baseline_ = FLT_MAX;
    double ideographic_baseline_ = FLT_MAX;

    bool needs_layout_ = true;

    struct WaveCoordinates {
        double x_start;
        double y_start;
        double x_end;
        double y_end;

        WaveCoordinates(double x_start, double y_start, double x_end, double y_end)
              : x_start(x_start), y_start(y_start), x_end(x_end), y_end(y_end) {}
    };

    // Passes in the text and Styled Runs. text_ and runs_ will later be passed
    // into breaker_ in InitBreaker(), which is called in Layout().
    void SetText(std::vector<uint16_t> text, StyledRuns runs);

    void SetParagraphStyle(const ParagraphStyle& style);
    void SetFontCollection(std::shared_ptr<FontCollection> font_collection);
    void SetInlinePlaceholders(std::vector<PlaceholderRun> inline_placeholders,
                               std::unordered_set<size_t> obj_replacement_char_indexes);
    // Break the text into lines.
    bool ComputeLineBreaks();

    // Break the text into runs based on LTR/RTL text direction.
    bool ComputeBidiRuns(std::vector<BidiRun>& result);

    // TODO implement
    // Calculates and populates strut based on paragraph_style_ strut info.
    // void ComputeStrut(StrutMetrics* strut, SkFont& font);
    void ComputeStruct(StrutMetrics* strut, minikin::MinikinFont* font);

    // Adjusts the ascent and descent based on the existence and type of
    // placeholder. This method sets the proper metrics to achieve the different
    // PlaceholderAlignment options.
    void ComputePlaceholder(PlaceholderRun* placeholder_run, double& ascent, double& descent);

    bool IsStrutValid() const;

    // TODO implement
    // UpdateLineMetrics

    // Calculate the starting X offset of a line based on the line's width and
    // alignment.
    double GetLineXOffset(double line_total_advance, bool justify_line);

    // TODO implement
    // void PaintDecorations
    // void ComputeWavyDecoration
    // void PaintBackground
    // void PaintShadow

    // Obtain a Minikin font collection matching this text style.
    std::shared_ptr<minikin::FontCollection> GetMinikinFontCollectionForStyle(
            const TextStyle& style);

    // TODO implement
    // GetDefaultTyleface
};

} // namespace txt
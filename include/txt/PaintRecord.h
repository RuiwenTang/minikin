//
// Created by TangRuiwen on 2020/10/10.
//

#ifndef MINIKIN_INCLUDE_TXT_PAINTRECORD_H_
#define MINIKIN_INCLUDE_TXT_PAINTRECORD_H_

#include <minikin/MinikinFont.h>
#include <txt/ParagraphStyle.h>

#include <vector>

namespace txt {

class PlaceholderRun;
// PaintRecord holds the layout data after Paragraph::Layout() is called. This
// stores all necessary offsets, blobs, metrics, and more for draw the text.
class PaintRecord {
public:
    ~PaintRecord() = default;

    PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs, float x_offset,
                float y_offset, minikin::MinikinFont::FontMetrics metrics, size_t line,
                float x_start, float x_end, bool is_ghost);

    PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs, float x_offset, float y_offset,
                minikin::MinikinFont::FontMetrics metrics, size_t line, float x_start, float x_end,
                bool is_ghost, PlaceholderRun* placeholder_run);

    PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs,
                minikin::MinikinFont::FontMetrics metrics, size_t line, float x_start, float x_end,
                bool is_ghost);

    float XOffset() const { return x_offset_; }
    float YOffset() const { return y_offset_; }
    void SetOffset(float x_offset, float y_offset) {
        x_offset_ = x_offset;
        y_offset_ = y_offset;
    }

    const minikin::MinikinFont::FontMetrics& Metrics() const { return metrics_; }
    const TextStyle& Style() const { return style_; }
    size_t line() const { return line_; }

    float XStart() const { return x_start_; }
    float XEnd() const { return x_end_; }
    float GetRunWidth() const { return x_end_ - x_start_; }
    PlaceholderRun* GetPlaceHolderRun() const { return placeholder_run_; }
    bool IsGhost() const { return is_ghost_; }
    bool isPlaceholder() const { return placeholder_run_ == nullptr; }

protected:
    TextStyle style_;
    std::vector<uint32_t> glyphs_;
    float x_offset_ = 0.f;
    float y_offset_ = 0.f;

    minikin::MinikinFont::FontMetrics metrics_;
    size_t line_;

    float x_start_ = 0.f;
    float x_end_ = 0.f;
    // 'Ghost' runs represent trailing whitespace. 'Ghost' runs should not have
    // decorations painted on them and do not impact layout of visible glyphs.
    bool is_ghost_ = false;
    PlaceholderRun* placeholder_run_ = nullptr;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_PAINTRECORD_H_

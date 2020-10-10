//
// Created by TangRuiwen on 2020/10/10.
//

#include <txt/PaintRecord.h>

namespace txt {

PaintRecord::PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs,
                         minikin::MinikinFont::FontMetrics metrics, size_t line, float x_start,
                         float x_end, bool is_ghost)
      : style_(style),
        glyphs_(std::move(glyphs)),
        metrics_(metrics),
        line_(line),
        x_start_(x_start),
        x_end_(x_end),
        is_ghost_(is_ghost) {}

PaintRecord::PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs, float x_offset,
                         float y_offset, minikin::MinikinFont::FontMetrics metrics, size_t line,
                         float x_start, float x_end, bool is_ghost)
      : style_(style),
        glyphs_(std::move(glyphs)),
        x_offset_(x_offset),
        y_offset_(y_offset),
        metrics_(metrics),
        line_(line),
        x_start_(x_start),
        x_end_(x_end),
        is_ghost_(is_ghost) {}

PaintRecord::PaintRecord(const TextStyle& style, std::vector<uint32_t> glyphs, float x_offset,
                         float y_offset, minikin::MinikinFont::FontMetrics metrics, size_t line,
                         float x_start, float x_end, bool is_ghost, PlaceholderRun* placeholder_run)
      : style_(style),
        glyphs_(std::move(glyphs)),
        x_offset_(x_offset),
        y_offset_(y_offset),
        metrics_(metrics),
        line_(line),
        x_start_(x_start),
        x_end_(x_end),
        is_ghost_(is_ghost),
        placeholder_run_(placeholder_run) {}

} // namespace txt
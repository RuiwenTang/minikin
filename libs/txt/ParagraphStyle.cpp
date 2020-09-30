//
// Created by TangRuiwen on 2020/9/30.
//

#include <txt/ParagraphStyle.h>
#include <txt/Platform.h>

namespace txt {

TextStyle::TextStyle() : font_families(Platform::GetDefaultFontFamilies()) {}

TextStyle ParagraphStyle::GetTextStyle() const {
    TextStyle result;
    result.font_weight = this->font_weight;
    result.font_style = this->font_style;
    result.font_families = std::vector<std::string>({this->font_family});
    if (this->font_size >= 0) {
        result.font_size = this->font_size;
    }
    result.local = this->local;
    result.height = this->height;
    result.has_height_override = this->has_height_override;

    return result;
}

bool ParagraphStyle::unlimited_lines() const {
    return this->max_lines == std::numeric_limits<size_t>::max();
}

bool ParagraphStyle::ellipsized() const {
    return !ellipsis.empty();
}

TextAlign ParagraphStyle::effective_align() const {
    if (text_align == TextAlign::kStart) {
        return (text_direction == TextDirection::kLtr) ? TextAlign::kLeft : TextAlign::kRight;
    } else if (text_align == TextAlign::kEnd) {
        return (text_direction == TextDirection::kLtr) ? TextAlign::kRight : TextAlign::kLeft;
    } else {
        return text_align;
    }
}

} // namespace txt
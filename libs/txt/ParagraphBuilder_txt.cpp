//
// Created by TangRuiwen on 2020/10/01.
//

#include "ParagraphBuilder_txt.h"

#include "ParagraphTxt.h"

namespace txt {

ParagraphBuilderTxt::ParagraphBuilderTxt(const ParagraphStyle &style,
                                         std::shared_ptr<FontCollection> font_collection)
      : font_collection_(std::move(font_collection)) {
    SetParagraphStyle(style);
}

ParagraphBuilderTxt::~ParagraphBuilderTxt() noexcept = default;

void ParagraphBuilderTxt::SetParagraphStyle(const ParagraphStyle &style) {
    paragraph_style_ = style;
    paragraph_style_index_ = runs_.AddStye(style.GetTextStyle());
    runs_.StartRun(paragraph_style_index_, text_.size());
}

void ParagraphBuilderTxt::PushStyle(const TextStyle &style) {
    size_t style_index = runs_.AddStye(style);
    style_stack_.emplace_back(style_index);
    runs_.StartRun(style_index, text_.size());
}

void ParagraphBuilderTxt::Pop() {
    if (style_stack_.empty()) {
        return;
    }

    style_stack_.pop_back();
    runs_.StartRun(PeekStyleIndex(), text_.size());
}

size_t ParagraphBuilderTxt::PeekStyleIndex() const {
    return style_stack_.empty() ? paragraph_style_index_ : style_stack_.back();
}

const TextStyle &ParagraphBuilderTxt::PeekStyle() {
    return runs_.GetStyle(PeekStyleIndex());
}

void ParagraphBuilderTxt::AddText(const std::u16string &text) {
    text_.insert(text_.end(), text.begin(), text.end());
}

void ParagraphBuilderTxt::AddPlaceholder(PlaceholderRun &span) {
    obj_replacement_char_indexes_.insert(text_.size());
    runs_.StartRun(PeekStyleIndex(), text_.size());
    AddText(std::u16string{1ull, objReplacementChar});
    runs_.StartRun(PeekStyleIndex(), text_.size());
    inline_placeholders_.emplace_back(span);
}

std::unique_ptr<Paragraph> ParagraphBuilderTxt::Build() {
    runs_.EndRunIfNeeded(text_.size());
    std::unique_ptr<ParagraphTxt> paragraph = std::make_unique<ParagraphTxt>();
    paragraph->SetText(std::move(text_), std::move(runs_));
    paragraph->SetInlinePlaceholders(std::move(inline_placeholders_),
                                     std::move(obj_replacement_char_indexes_));
    paragraph->SetParagraphStyle(paragraph_style_);
    paragraph->SetFontCollection(font_collection_);
    SetParagraphStyle(paragraph_style_);
    return paragraph;
}

} // namespace txt

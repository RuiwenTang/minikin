//
// Created by TangRuiwen on 2020/10/01.
//

#ifndef MINIKIN_LIBS_TXT_PARAGRAPHBUILDER_TXT_H
#define MINIKIN_LIBS_TXT_PARAGRAPHBUILDER_TXT_H

#include <txt/FontCollection.h>
#include <txt/ParagraphBuilder.h>
#include <txt/ParagraphStyle.h>
#include <txt/StyledRuns.h>

#include <memory>
#include <unordered_set>
#include <vector>

namespace txt {
// Implemention of ParagraphBuilder that produces paragraphs backed by the minikin text layout
// library.
class ParagraphBuilderTxt : public ParagraphBuilder {
public:
    ParagraphBuilderTxt(const ParagraphStyle& style,
                        std::shared_ptr<FontCollection> font_collection);
    ~ParagraphBuilderTxt() override;

    void PushStyle(const TextStyle& style) override;
    void Pop() override;
    const TextStyle& PeekStyle() override;
    void AddText(const std::u16string& text) override;
    void AddPlaceholder(PlaceholderRun& span) override;
    std::unique_ptr<Paragraph> Build() override;

private:
    void SetParagraphStyle(const ParagraphStyle& style);
    size_t PeekStyleIndex() const;

private:
    std::vector<uint16_t> text_;
    // A vector of PlaceholderRuns, which detail the sizes, positioning and break
    // behavior of the empty spaces to leave. Each placeholder span corresponds to
    // a 0xFFFC (object replacement character) in text_, which indicates the
    // position in the text where the placeholder will occur. There should be an
    // equal number of 0xFFFC characters and elements in this vector.
    std::vector<PlaceholderRun> inline_placeholders_;
    // The indexes of the obj replacement characters added through
    // ParagraphBuilder::addPlaceholder().
    std::unordered_set<size_t> obj_replacement_char_indexes_;
    std::vector<size_t> style_stack_;
    std::shared_ptr<FontCollection> font_collection_;
    StyledRuns runs_;

    ParagraphStyle paragraph_style_;
    size_t paragraph_style_index_;
};
} // namespace txt

#endif
//
// Created by TangRuiwen on 2020/9/30.
//

#ifndef MINIKIN_INCLUDE_TXT_PARAGRAPHBUILDER_H_
#define MINIKIN_INCLUDE_TXT_PARAGRAPHBUILDER_H_

#include <txt/Paragraph.h>
#include <txt/ParagraphStyle.h>
#include <txt/PlaceHolderRun.h>

#include <memory>

namespace txt {

class ParagraphBuilder {
public:
    static std::unique_ptr<ParagraphBuilder> CreateTxtBuilder(const ParagraphStyle& style);

    virtual ~ParagraphBuilder() = default;

    // Push a style to the stack. The corresponding text added with AddText will
    // use the top-most style.
    virtual void PushStyle(const TextStyle& style) = 0;

    // Remove a style from the stack. Useful to apply different styles to chunks
    // of text such as bolding.
    // Example:
    //   builder.PushStyle(normal_style);
    //   builder.AddText("Hello this is normal. ");
    //
    //   builder.PushStyle(bold_style);
    //   builder.AddText("And this is BOLD. ");
    //
    //   builder.Pop();
    //   builder.AddText(" Back to normal again.");
    virtual void Pop() = 0;

    // Returns the last TextStyle on the stack.
    virtual const TextStyle& PeekStyle() = 0;

    // Adds text to the builder. Forms the proper runs to use the upper-most style
    // on the style_stack_;
    virtual void AddText(const std::u16string& text) = 0;
    virtual void AddPlaceholder(PlaceholderRun& span) = 0;

    virtual std::unique_ptr<Paragraph> Build() = 0;

protected:
    ParagraphBuilder() = default;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_PARAGRAPHBUILDER_H_

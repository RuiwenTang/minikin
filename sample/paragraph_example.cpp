//
// Created by TangRuiwen on 2020/9/30.
//

#include <txt/FontCollection.h>
#include <txt/ParagraphBuilder.h>
#include <txt/ParagraphStyle.h>

#include <iostream>

int main(int argc, const char** argv) {
    txt::TextStyle style;
    style.font_size = 20;
    style.font_families.emplace_back("Roboto");

    txt::ParagraphStyle paragraph_style;

    auto builder = txt::ParagraphBuilder::CreateTxtBuilder(paragraph_style);
    builder->PushStyle(style);
    std::string raw_string =
            "fine world \xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87";
    icu::UnicodeString unicode_string(raw_string.c_str());
    builder->AddText(std::u16string{unicode_string.getBuffer()});

    auto paragraph = builder->Build();
    paragraph->Layout(300);

    double max_width = paragraph->GetMaxWidth();
    auto line_metrics = paragraph->GetLineMetrics();

    for (auto& line : line_metrics) {
        std::string s;
        std::cout << "line content : "
                  << unicode_string.tempSubString(line.start_index, line.end_index).toUTF8String(s)
                  << std::endl;
    }

    std::cout << "max_width = " << max_width << std::endl;
    std::cout << "GetMinIntrinsicWidth = " << paragraph->GetMinIntrinsicWidth() << std::endl;
    std::cout << "GetMaxIntrinsicWidth = " << paragraph->GetMaxIntrinsicWidth() << std::endl;
    return 0;
}
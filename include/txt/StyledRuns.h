//
// Created by TangRuiwen on 2020/10/01.
//

#ifndef MINIKIN_INCLUDE_TXT_STYLEDRUNS_H_
#define MINIKIN_INCLUDE_TXT_STYLEDRUNS_H_
#include <txt/ParagraphStyle.h>

#include <vector>

namespace txt {

// This holds and handles the start/end positions of discrete chunks of text
// that use different styles (a 'run').
class StyledRuns final {
public:
    struct Run {
        const TextStyle& style;
        size_t start;
        size_t end;
    };

    StyledRuns();
    StyledRuns(StyledRuns&& other) noexcept;
    ~StyledRuns();

    const StyledRuns& operator=(StyledRuns&& other) noexcept;

    void Swap(StyledRuns& other);

    size_t AddStye(const TextStyle& style);

    const TextStyle& GetStyle(size_t style_index) const;

    void StartRun(size_t style_index, size_t start);

    void EndRunIfNeeded(size_t end);

    size_t Size() const { return runs_.size(); }

    Run GetRun(size_t index) const;

private:
    struct IndexedRun {
        size_t style_index = 0;
        size_t start = 0;
        size_t end = 0;

        IndexedRun(size_t style_index, size_t start, size_t end)
              : style_index(style_index), start(start), end(end) {}
    };

    std::vector<TextStyle> styles_;
    std::vector<IndexedRun> runs_;
};

} // namespace txt

#endif MINIKIN_INCLUDE_TXT_STYLEDRUNS_H_

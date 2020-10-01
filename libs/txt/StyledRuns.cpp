//
// Created by TangRuiwen on 2020/10/01.
//

#include <txt/StyledRuns.h>

namespace txt {

StyledRuns::StyledRuns() = default;

StyledRuns::StyledRuns(StyledRuns&& other) noexcept {
    styles_.swap(other.styles_);
    runs_.swap(other.runs_);
}

StyledRuns::~StyledRuns() = default;

const StyledRuns& StyledRuns::operator=(StyledRuns&& other) noexcept {
    styles_.swap(other.styles_);
    runs_.swap(other.runs_);
    return *this;
}

void StyledRuns::Swap(StyledRuns& other) {
    styles_.swap(other.styles_);
    runs_.swap(other.runs_);
}

size_t StyledRuns::AddStye(const TextStyle& style) {
    size_t style_index = styles_.size();
    styles_.emplace_back(style);
    return style_index;
}

const TextStyle& StyledRuns::GetStyle(size_t style_index) const {
    return styles_[style_index];
}

void StyledRuns::StartRun(size_t style_index, size_t start) {
    EndRunIfNeeded(start);
    runs_.emplace_back(IndexedRun{style_index, start, start});
}

void StyledRuns::EndRunIfNeeded(size_t end) {
    if (runs_.empty()) return;

    IndexedRun& run = runs_.back();
    if (run.start == end) {
        // The run is empty. We can skip it.
        runs_.pop_back();
    } else {
        run.end = end;
    }
}

StyledRuns::Run StyledRuns::GetRun(size_t index) const {
    const IndexedRun& run = runs_[index];
    return Run{styles_[run.style_index], run.start, run.end};
}

} // namespace txt

//
// Created by TangRuiwen on 2020/10/9.
//

#include <txt/FontManager.h>
#include <txt/Typeface.h>

#include <atomic>

namespace txt {

Typeface::Typeface(const FontStyle &style) : style_(style), unique_id_(NextID()) {}

uint32_t Typeface::NextID() {
    static std::atomic<uint32_t> nextID{};
    return nextID++;
}

std::shared_ptr<TypefaceFont> TypefaceFont::MakeFont(const std::shared_ptr<Typeface> &typeface) {
    return FontManager::Default()->MakeFont(typeface);
}

} // namespace txt
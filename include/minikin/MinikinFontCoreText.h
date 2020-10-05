//
// Created by TangRuiwen on 2020/9/29.
//

#ifndef MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_
#define MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_

#include <CoreText/CoreText.h>
#include <minikin/MinikinFont.h>

namespace minikin {

class MinikinFontCoreText : public minikin::MinikinFont {
public:
    explicit MinikinFontCoreText(CTFontRef ct_font);
    ~MinikinFontCoreText() noexcept override;

    float GetHorizontalAdvance(uint32_t glyph_id,
                               const minikin::MinikinPaint &paint) const override;
    void GetBounds(minikin::MinikinRect *bounds, uint32_t glyph_id,
                   const minikin::MinikinPaint &paint) const override;
    const void *GetTable(uint32_t tag, size_t *size, minikin::MinikinDestroyFunc *destroy) override;
    size_t GetFontSize() const override;
    bool Render(uint32_t glyph_id, const minikin::MinikinPaint &paint,
                minikin::GlyphBitmap *result) override;

private:
    static uint64_t sIdCounter;

    CTFontRef ct_font_;
};

} // namespace minikin

#endif // MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_

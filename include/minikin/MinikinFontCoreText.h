//
// Created by TangRuiwen on 2020/9/29.
//

#ifndef MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_
#define MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_

#include <CoreText/CoreText.h>
#include <minikin/MinikinFont.h>

namespace minikin {

class MinikinFontCoreText : public android::MinikinFont {
public:
    explicit MinikinFontCoreText(CTFontRef ct_font);
    ~MinikinFontCoreText() override;

    float GetHorizontalAdvance(uint32_t glyph_id,
                               const android::MinikinPaint &paint) const override;
    void GetBounds(android::MinikinRect *bounds, uint32_t glyph_id,
                   const android::MinikinPaint &paint) const override;
    const void *GetTable(uint32_t tag, size_t *size, android::MinikinDestroyFunc *destroy) override;
    const void *GetFontData() const override;
    size_t GetFontSize() const override;
    int GetFontIndex() const override;
    bool Render(uint32_t glyph_id, const android::MinikinPaint &paint,
                android::GlyphBitmap *result) override;

private:
    static uint64_t sIdCounter;

    CTFontRef ct_font_;
};

} // namespace minikin

#endif // MINIKIN_INCLUDE_MINIKIN_MINIKINFONTCORETEXT_H_

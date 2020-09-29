//
// Created by TangRuiwen on 2020/9/29.
//

#include <hb-coretext.h>
#include <hb.h>
#include <minikin/MinikinFontCoreText.h>

namespace minikin {

uint64_t MinikinFontCoreText::sIdCounter = 0;

MinikinFontCoreText::MinikinFontCoreText(CTFontRef ct_font)
      : android::MinikinFont(sIdCounter++), ct_font_(ct_font) {}

MinikinFontCoreText::~MinikinFontCoreText() noexcept {
    CFRelease(ct_font_);
}

float MinikinFontCoreText::GetHorizontalAdvance(uint32_t glyph_id,
                                                const android::MinikinPaint &paint) const {
    CGAffineTransform matrix = CTFontGetMatrix(ct_font_);
    CTFontRef font = CTFontCreateCopyWithAttributes(ct_font_, (double)paint.size,
                                                    std::addressof(matrix), nullptr);

    CGSize size;
    CTFontGetAdvancesForGlyphs(font, CTFontOrientation::kCTFontOrientationDefault,
                               reinterpret_cast<const CGGlyph *>(std::addressof(glyph_id)),
                               std::addressof(size), 1);

    CFRelease(font);
    return size.width;
}

void MinikinFontCoreText::GetBounds(android::MinikinRect *bounds, uint32_t glyph_id,
                                    const android::MinikinPaint &paint) const {}

const void *MinikinFontCoreText::GetTable(uint32_t tag, size_t *size,
                                          android::MinikinDestroyFunc *destroy) {

    hb_font_t* hb_font = hb_coretext_font_create(ct_font_);
    hb_face_t* hb_face = hb_font_get_face(hb_font);
    hb_blob_t* blob = hb_face_reference_table(hb_face, tag);

    hb_font_destroy(hb_font);
    return hb_blob_get_data(blob, reinterpret_cast<unsigned int *>(size));

}

size_t MinikinFontCoreText::GetFontSize() const {
    return CTFontGetSize(ct_font_);
}

bool MinikinFontCoreText::Render(uint32_t glyph_id, const android::MinikinPaint &paint,
                                 android::GlyphBitmap *result) {
    return true;
}

} // namespace minikin
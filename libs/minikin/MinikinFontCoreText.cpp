//
// Created by TangRuiwen on 2020/9/29.
//

#include <hb-coretext.h>
#include <hb.h>
#include <minikin/MinikinFontCoreText.h>

namespace minikin {

uint64_t MinikinFontCoreText::sIdCounter = 0;

MinikinFontCoreText::MinikinFontCoreText(CTFontRef ct_font)
      : minikin::MinikinFont(sIdCounter++), ct_font_(ct_font) {
    CFRetain(ct_font_);
}

MinikinFontCoreText::~MinikinFontCoreText() noexcept {
    CFRelease(ct_font_);
}

float MinikinFontCoreText::GetHorizontalAdvance(uint32_t glyph_id,
                                                const minikin::MinikinPaint &paint) const {
    CTFontRef font = CTFontCreateCopyWithAttributes(ct_font_, (double)paint.size, nullptr, nullptr);

    CGSize size;
    CTFontGetAdvancesForGlyphs(font, CTFontOrientation::kCTFontOrientationHorizontal,
                               reinterpret_cast<const CGGlyph *>(std::addressof(glyph_id)),
                               std::addressof(size), 1);

    CFRelease(font);
    return size.width;
}

void MinikinFontCoreText::GetBounds(minikin::MinikinRect *bounds, uint32_t glyph_id,
                                    const minikin::MinikinPaint &paint) const {
    CTFontRef font = ct_font_;
    if (paint.size == CTFontGetSize(ct_font_)) {
        CFRetain(font);
    } else {
        font = CTFontCreateCopyWithAttributes(ct_font_, (double)paint.size, nullptr, nullptr);
    }

    CGRect rect;
    CGGlyph cg_glyph = glyph_id;
    CTFontGetBoundingRectsForGlyphs(font, CTFontOrientation::kCTFontOrientationHorizontal,
                                    std::addressof(cg_glyph), std::addressof(rect), 1);
    bounds->mLeft = rect.origin.x;
    bounds->mTop = rect.origin.y;
    bounds->mBottom = rect.size.height;
    bounds->mRight = rect.size.width;
    CFRelease(font);
}

const void *MinikinFontCoreText::GetTable(uint32_t tag, size_t *size,
                                          minikin::MinikinDestroyFunc *destroy) {
    hb_font_t *hb_font = hb_coretext_font_create(ct_font_);
    hb_face_t *hb_face = hb_font_get_face(hb_font);
    hb_blob_t *blob = hb_face_reference_table(hb_face, tag);

    hb_font_destroy(hb_font);
    return hb_blob_get_data(blob, reinterpret_cast<unsigned int *>(size));
}



bool MinikinFontCoreText::Render(uint32_t glyph_id, const minikin::MinikinPaint &paint,
                                 minikin::GlyphBitmap *result) {
    return true;
}

} // namespace minikin
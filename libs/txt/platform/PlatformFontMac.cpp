//
// Created by TangRuiwen on 2020/10/9.
//
#include <CoreText/CoreText.h>
#include <dlfcn.h>
#include <hb-coretext.h>
#include <hb.h>
#include <txt/FontManager.h>
#include <txt/Typeface.h>

#include <cmath>
#include <mutex>

#include "CTFontManager.h"

namespace txt {

template <typename S, typename D, typename C>
struct LinearInterpolater {
    struct Mapping {
        S src_val;
        D dst_val;
    };
    constexpr LinearInterpolater(Mapping const mapping[], int mappingCount)
          : fMapping(mapping), fMappingCount(mappingCount) {}

    static D map(S value, S src_min, S src_max, D dst_min, D dst_max) {
        assert(src_min < src_max);
        assert(dst_min <= dst_max);
        return C()(dst_min + (((value - src_min) * (dst_max - dst_min)) / (src_max - src_min)));
    }

    D map(S val) const {
        // -Inf to [0]
        if (val < fMapping[0].src_val) {
            return fMapping[0].dst_val;
        }

        // Linear from [i] to [i+1]
        for (int i = 0; i < fMappingCount - 1; ++i) {
            if (val < fMapping[i + 1].src_val) {
                return map(val, fMapping[i].src_val, fMapping[i + 1].src_val, fMapping[i].dst_val,
                           fMapping[i + 1].dst_val);
            }
        }

        // From [n] to +Inf
        // if (fcweight < Inf)
        return fMapping[fMappingCount - 1].dst_val;
    }

    Mapping const* fMapping;
    int fMappingCount;
};

struct CGFloatIdentity {
    CGFloat operator()(CGFloat s) { return s; }
};

struct RoundCGFloatToUInt {
    uint32_t operator()(CGFloat s) { return s + 0.5; }
};

static inline int32_t sqr(int32_t value) {
    assert(std::abs(value) < 0x7FFF);
    return value * value;
}

static int32_t compute_metric(const FontStyle& a, const FontStyle& b) {
    return sqr(a.GetWeight() - b.GetWeight()) + sqr((a.GetWidth() - b.GetWidth()) * 100) +
            sqr((a.GetSlant() != b.GetSlant()) * 900);
}

/** Returns the [-1, 1] CTFontDescriptor weights for the
 *  <0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000> CSS weights.
 *
 *  It is assumed that the values will be interpolated linearly between these points.
 *  NSFontWeightXXX were added in 10.11, appear in 10.10, but do not appear in 10.9.
 *  The actual values appear to be stable, but they may change in the future without notice.
 */
static CGFloat (&get_NSFontWeight_mapping())[11] {
    // Declarations in <AppKit/AppKit.h> on macOS, <UIKit/UIKit.h> on iOS
#ifdef BUILD_FOR_MAC
#define FONT_WEIGHT_PREFIX "NS"
#endif
#ifdef BUILD_FOR_IOS
#define FONT_WEIGHT_PREFIX "UI"
#endif
    static constexpr struct {
        CGFloat defaultValue;
        const char* name;
    } nsFontWeightLoaderInfos[] = {
            {-0.80f, FONT_WEIGHT_PREFIX "FontWeightUltraLight"},
            {-0.60f, FONT_WEIGHT_PREFIX "FontWeightThin"},
            {-0.40f, FONT_WEIGHT_PREFIX "FontWeightLight"},
            {0.00f, FONT_WEIGHT_PREFIX "FontWeightRegular"},
            {0.23f, FONT_WEIGHT_PREFIX "FontWeightMedium"},
            {0.30f, FONT_WEIGHT_PREFIX "FontWeightSemibold"},
            {0.40f, FONT_WEIGHT_PREFIX "FontWeightBold"},
            {0.56f, FONT_WEIGHT_PREFIX "FontWeightHeavy"},
            {0.62f, FONT_WEIGHT_PREFIX "FontWeightBlack"},
    };

    static CGFloat nsFontWeights[11];
    static std::once_flag once_flag;
    std::call_once(once_flag, [&] {
        size_t i = 0;
        nsFontWeights[i++] = -1.00;
        for (const auto& nsFontWeightLoaderInfo : nsFontWeightLoaderInfos) {
            void* nsFontWeightValuePtr = dlsym(RTLD_DEFAULT, nsFontWeightLoaderInfo.name);
            if (nsFontWeightValuePtr) {
                nsFontWeights[i++] = *(static_cast<CGFloat*>(nsFontWeightValuePtr));
            } else {
                nsFontWeights[i++] = nsFontWeightLoaderInfo.defaultValue;
            }
        }
        nsFontWeights[i++] = 1.00;
    });
    return nsFontWeights;
}

/** Convert the [0, 1000] CSS weight to [-1, 1] CTFontDescriptor weight (for system fonts).
 *
 *  The -1 to 1 weights reported by CTFontDescriptors have different mappings depending on if the
 *  CTFont is native or created from a CGDataProvider.
 */
static CGFloat font_style_to_ct_weight(uint32_t font_style_weight) {
    using Interpolator = LinearInterpolater<uint32_t, CGFloat, CGFloatIdentity>;

    static Interpolator::Mapping nativeWeightMappings[11];
    static std::once_flag once_flag;
    std::call_once(once_flag, [&] {
        CGFloat(&nsFontWeights)[11] = get_NSFontWeight_mapping();
        for (int i = 0; i < 11; ++i) {
            nativeWeightMappings[i].src_val = i * 100;
            nativeWeightMappings[i].dst_val = nsFontWeights[i];
        }
    });

    static constexpr Interpolator nativeInterpolator(nativeWeightMappings, 11);

    return nativeInterpolator.map(font_style_weight);
}

/** Convert the [-1, 1] CTFontDescriptor weight to [0, 1000] CSS weight.
 *
 *  The -1 to 1 weights reported by CTFontDescriptors have different mappings depending on if the
 *  CTFont is native or created from a CGDataProvider.
 */
static uint32_t ct_weight_to_font_style(CGFloat cg_weight) {
    using Interpolator = LinearInterpolater<CGFloat, uint32_t, RoundCGFloatToUInt>;

    static Interpolator::Mapping nativeWeightMappings[11];

    static std::once_flag once_flag;
    std::call_once(once_flag, [&] {
        CGFloat(&nsFontWeights)[11] = get_NSFontWeight_mapping();
        for (int i = 0; i < 11; ++i) {
            nativeWeightMappings[i].src_val = nsFontWeights[i];
            nativeWeightMappings[i].dst_val = i * 100;
        }
    });

    static constexpr Interpolator nativeInterpolator(nativeWeightMappings, 11);
    return nativeInterpolator.map(cg_weight);
}

static uint32_t ct_width_to_font_style(CGFloat cg_width) {
    using Interpolator = LinearInterpolater<CGFloat, int, RoundCGFloatToUInt>;

    // Values determined by creating font data with every width, creating a CTFont,
    // and asking the CTFont for its width. See TypefaceStyle test for basics.
    static constexpr Interpolator::Mapping widthMappings[] = {
            {-0.5, 0},
            {0.5, 10},
    };
    static constexpr Interpolator interpolator(widthMappings, 2);
    return interpolator.map(cg_width);
}

static int32_t font_style_to_ct_width(int32_t fontstyleWidth) {
    using Interpolator = LinearInterpolater<int, CGFloat, CGFloatIdentity>;

    // Values determined by creating font data with every width, creating a CTFont,
    // and asking the CTFont for its width. See TypefaceStyle test for basics.
    static constexpr Interpolator::Mapping widthMappings[] = {
            {0, -0.5},
            {10, 0.5},
    };
    static constexpr Interpolator interpolator(widthMappings, 2);
    return interpolator.map(fontstyleWidth);
}

static bool find_dict_CGFloat(CFDictionaryRef dict, CFStringRef name, CGFloat* value) {
    CFNumberRef num;
    return CFDictionaryGetValueIfPresent(dict, name, (const void**)&num) &&
            CFNumberIsFloatType(num) && CFNumberGetValue(num, kCFNumberCGFloatType, value);
}

static bool find_desc_str(CTFontDescriptorRef desc, CFStringRef name, std::string& value) {
    auto ref = (CFStringRef)CTFontDescriptorCopyAttribute(desc, name);
    if (!ref) {
        return false;
    }

    char buffer[50] = {0};
    CFStringGetCString(ref, buffer, 50, kCFStringEncodingUTF8);
    value = buffer;

    CFRelease(ref);
    return true;
}

static FontStyle font_style_from_descriptor(CTFontDescriptorRef desc) {
    CFTypeRef traits = CTFontDescriptorCopyAttribute(desc, kCTFontTraitsAttribute);
    if (!traits || CFDictionaryGetTypeID() != CFGetTypeID(traits)) {
        CFRelease(traits);
        return {};
    }

    auto font_traits_dict = static_cast<CFDictionaryRef>(traits);
    CGFloat weight, width, slant;

    if (!find_dict_CGFloat(font_traits_dict, kCTFontWeightTrait, &weight)) {
        weight = 0;
    }
    if (!find_dict_CGFloat(font_traits_dict, kCTFontWidthTrait, &width)) {
        width = 0;
    }

    if (!find_dict_CGFloat(font_traits_dict, kCTFontSlantTrait, &slant)) {
        slant = 0;
    }

    return FontStyle{(FontStyle::Weight)ct_weight_to_font_style(weight),
                     (FontStyle::Width)ct_width_to_font_style(width),
                     slant ? FontStyle::kItalic_Slant : FontStyle::kUpright_Slant};
}

CTFontDescriptorRef create_descriptor(const std::string& name, const FontStyle& style) {
    CFMutableDictionaryRef cf_attributes =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);

    CFMutableDictionaryRef cf_traits =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);

    // TODO(crbug.com/1018581) Some CoreText versions have errant behavior when
    // certain traits set.  Temporary workaround to omit specifying trait for
    // those versions.
    // Long term solution will involve serializing typefaces instead of relying
    // upon this to match between processes.
    //
    // Compare CoreText.h in an up to date SDK for where these values come from.
    static const uint32_t kSkiaLocalCTVersionNumber10_14 = 0x000B0000;
    static const uint32_t kSkiaLocalCTVersionNumber10_15 = 0x000C0000;
    // CTFontTraits (symbolic)
    // macOS 14 and iOS 12 seem to behave badly when kCTFontSymbolicTrait is set.
    // macOS 15 yields LastResort font instead of a good default font when
    // kCTFontSymbolicTrait is set.
    if (!(&CTGetCoreTextVersion && CTGetCoreTextVersion() >= kSkiaLocalCTVersionNumber10_14)) {
        CTFontSymbolicTraits ctFontTraits = 0;
        if (style.GetWeight() >= FontStyle::kBold_Weight) {
            ctFontTraits |= kCTFontBoldTrait;
        }
        if (style.GetSlant() != FontStyle::kUpright_Slant) {
            ctFontTraits |= kCTFontItalicTrait;
        }
        CFNumberRef cfFontTraits =
                CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ctFontTraits);
        if (cfFontTraits) {
            CFDictionaryAddValue(cf_traits, kCTFontSymbolicTrait, cfFontTraits);
        }
        CFRelease(cfFontTraits);
    }

    // CTFontTraits (weight)
    CGFloat ctWeight = font_style_to_ct_weight(style.GetWeight());
    CFNumberRef cfFontWeight = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctWeight);
    if (cfFontWeight) {
        CFDictionaryAddValue(cf_traits, kCTFontWeightTrait, cfFontWeight);
    }
    CFRelease(cfFontWeight);

    // CTFontTraits (width)
    CGFloat ctWidth = font_style_to_ct_width(style.GetWidth());
    CFNumberRef cfFontWidth = CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctWidth);
    if (cfFontWidth) {
        CFDictionaryAddValue(cf_traits, kCTFontWidthTrait, cfFontWidth);
    }
    CFRelease(cfFontWidth);

    // CTFontTraits (slant)
    // macOS 15 behaves badly when kCTFontSlantTrait is set.
    if (!(&CTGetCoreTextVersion && CTGetCoreTextVersion() == kSkiaLocalCTVersionNumber10_15)) {
        CGFloat ctSlant = style.GetSlant() == FontStyle::kUpright_Slant ? 0 : 1;
        CFNumberRef cfFontSlant =
                CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctSlant);
        if (cfFontSlant) {
            CFDictionaryAddValue(cf_traits, kCTFontSlantTrait, cfFontSlant);
        }
        CFRelease(cfFontSlant);
    }

    // CTFontTraits
    CFDictionaryAddValue(cf_attributes, kCTFontTraitsAttribute, cf_traits);

    // CTFontFamilyName
    if (!name.empty()) {
        CFStringRef cfFontName =
                CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8);
        if (cfFontName) {
            CFDictionaryAddValue(cf_attributes, kCTFontFamilyNameAttribute, cfFontName);
        }
        CFRelease(cfFontName);
    }

    auto desc = CTFontDescriptorCreateWithAttributes(cf_attributes);
    CFRelease(cf_traits);
    CFRelease(cf_attributes);
    return desc;
}

class CTTypeface : public Typeface {
public:
    CTTypeface(CTFontRef ct_font, const FontStyle& style)
          : Typeface(style), ct_font_(static_cast<CTFontRef>(CFRetain(ct_font))) {}

    ~CTTypeface() override { CFRelease(ct_font_); }

    std::string GetFamilyName() override {
        if (!ct_font_) {
            return "";
        }

        CFStringRef cf_name = CTFontCopyFamilyName(ct_font_);
        if (!cf_name) {
            return "";
        }

        char buffer[50] = {0};
        CFStringGetCString(cf_name, buffer, 50, kCFStringEncodingUTF8);
        std::string family_name{buffer};

        CFRelease(cf_name);
        return family_name;
    }

    static std::shared_ptr<CTTypeface> CreateFromCTFontRef(CTFontRef font) {
        CTFontDescriptorRef desc = CTFontCopyFontDescriptor(font);
        FontStyle style = font_style_from_descriptor(desc);

        auto face = std::make_shared<CTTypeface>(font, style);
        CFRelease(desc);
        return face;
    }

    static std::shared_ptr<CTTypeface> CreateFromDesc(CTFontDescriptorRef desc) {
        CTFontRef ct_font = CTFontCreateWithFontDescriptor(desc, 0, nullptr);
        if (!ct_font) {
            return nullptr;
        }

        auto face = CreateFromCTFontRef(ct_font);
        CFRelease(ct_font);
        return face;
    }

private:
    CTFontRef ct_font_;
};

class CTFontStyleSet : public FontStyleSet {
public:
    explicit CTFontStyleSet(CTFontDescriptorRef desc)
          : FontStyleSet(),
            array_(CTFontDescriptorCreateMatchingFontDescriptors(desc, nullptr)),
            count_(0) {
        if (!array_) {
            array_ = CFArrayCreate(nullptr, nullptr, 0, nullptr);
        }

        count_ = CFArrayGetCount(array_);
    }

    ~CTFontStyleSet() override { CFRelease(array_); }

    uint32_t Count() const override { return count_; }

    std::string GetStyle(uint32_t index, FontStyle* style) override {
        assert(index < count_);

        auto desc = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(array_, index));

        if (style) {
            *style = font_style_from_descriptor(desc);
        }
        std::string name;
        find_desc_str(desc, kCTFontStyleNameAttribute, name);
        return name;
    }

    std::shared_ptr<Typeface> CreateTypeface(uint32_t index) override {
        if (index > count_) {
            return std::shared_ptr<Typeface>();
        }

        auto desc = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(array_, index));
        return CTTypeface::CreateFromDesc(desc);
    }

    std::shared_ptr<Typeface> MatchStyle(const FontStyle& style) override {
        if (count_ == 0) {
            return std::shared_ptr<Typeface>();
        }

        return CTTypeface::CreateFromDesc(FindMatchingDesc(style));
    }

private:
    CTFontDescriptorRef FindMatchingDesc(const FontStyle& pattern) const {
        int32_t bestMetric = std::numeric_limits<int32_t>::max();
        CTFontDescriptorRef bestDesc = nullptr;

        for (uint32_t i = 0; i < count_; i++) {
            auto desc = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(array_, i));
            int32_t metric = compute_metric(pattern, font_style_from_descriptor(desc));
            if (0 == metric) {
                return desc;
            }

            if (metric < bestMetric) {
                bestMetric = metric;
                bestDesc = desc;
            }
        }
        assert(bestDesc);
        return bestDesc;
    }

private:
    CFArrayRef array_;
    uint32_t count_;
};

CTFontManager::CTFontManager()
      : FontManager(), names_(CopyAvailableFontFamilyNames()), count_(CFArrayGetCount(names_)) {}

CTFontManager::~CTFontManager() {
    CFRelease(names_);
}

std::shared_ptr<FontStyleSet> CTFontManager::MatchFamily(const std::string& family_name) const {
    if (family_name.empty()) {
        return nullptr;
    }
    CFStringRef cf_name = CFStringCreateWithCString(CFAllocatorGetDefault(), family_name.c_str(),
                                                    kCFStringEncodingUTF8);
    auto style_set = CreateSet(cf_name);
    CFRelease(cf_name);
    return style_set;
}

std::shared_ptr<Typeface> CTFontManager::MatchFamilyStyle(const std::string& family_name,
                                                          const FontStyle& style) const {
    CTFontDescriptorRef desc = create_descriptor(family_name, style);

    auto typeface = CTTypeface::CreateFromDesc(desc);
    CFRelease(desc);
    return typeface;
}

std::shared_ptr<Typeface> CTFontManager::MatchFamilyStyleCharacter(const std::string& family_name,
                                                                   const FontStyle& style,
                                                                   const std::string& locale,
                                                                   uint32_t character) const {
    CTFontDescriptorRef desc = create_descriptor(family_name, style);
    CTFontRef familyFont = CTFontCreateWithFontDescriptor(desc, 0, nullptr);
    CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                 reinterpret_cast<const uint8_t*>(&character),
                                                 sizeof(character), kCFStringEncodingUTF32, false);
    // If 0xD800 <= codepoint <= 0xDFFF || 0x10FFFF < codepoint 'string' may be nullptr.
    // No font should be covering such codepoints (even the magic fallback font).
    if (!string) {
        CFRelease(desc);
        CFRelease(familyFont);
    }

    CFRange range = CFRangeMake(0, CFStringGetLength(string));
    CTFontRef fallbackFont = CTFontCreateForString(familyFont, string, range);
    auto typeface = CTTypeface::CreateFromCTFontRef(fallbackFont);

    CFRelease(desc);
    CFRelease(familyFont);
    CFRelease(string);
    CFRelease(fallbackFont);
    return typeface;
}

CFArrayRef CTFontManager::CopyAvailableFontFamilyNames() {
#ifdef BUILD_FOR_MAC
    return CTFontManagerCopyAvailableFontFamilyNames();
#else

#endif
}

std::shared_ptr<Typeface> CTFontManager::MakeFromFile(const std::string& path) {
    return nullptr;
}

std::shared_ptr<Typeface> CTFontManager::MakeFromName(const std::string& family_name,
                                                      const FontStyle& style) {
    return nullptr;
}

std::shared_ptr<FontStyleSet> CTFontManager::CreateSet(CFStringRef cf_family_name) {
    CFMutableDictionaryRef cfAttr =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(cfAttr, kCTFontFamilyNameAttribute, cf_family_name);

    CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(cfAttr);
    auto style_set = std::make_shared<CTFontStyleSet>(desc);
    CFRelease(desc);
    CFRelease(cfAttr);
    return style_set;
}

class CTTypefaceFont : public TypefaceFont {
public:
    explicit CTTypefaceFont(const std::shared_ptr<CTTypeface>& typeface) : TypefaceFont(typeface) {
        ct_font_ = CTFontCreateWithFontDescriptor(create_descriptor(typeface->GetFamilyName(),
                                                                    typeface->GetStyle()),
                                                  0, nullptr);
    }

    ~CTTypefaceFont() override { CFRelease(ct_font_); }

    float GetHorizontalAdvance(uint32_t glyph_id,
                               const minikin::MinikinPaint& paint) const override {
        CTFontRef font = ct_font_;
        if (paint.size == CTFontGetSize(ct_font_)) {
            CFRetain(font);
        } else {
            auto desc = create_descriptor(typeface_->GetFamilyName(), typeface_->GetStyle());
            font = CTFontCreateCopyWithAttributes(ct_font_, paint.size, nullptr, desc);
            CFRelease(desc);
        }

        CGSize size;
        CTFontGetAdvancesForGlyphs(font, CTFontOrientation::kCTFontOrientationHorizontal,
                                   reinterpret_cast<const CGGlyph*>(std::addressof(glyph_id)),
                                   std::addressof(size), 1);

        CFRelease(font);
        return size.width;
    }

    void GetBounds(minikin::MinikinRect* bounds, uint32_t glyph_id,
                   const minikin::MinikinPaint& paint) const override {
        CTFontRef font = ct_font_;
        if (paint.size == CTFontGetSize(ct_font_)) {
            CFRetain(font);
        } else {
            auto desc = create_descriptor(typeface_->GetFamilyName(), typeface_->GetStyle());
            font = CTFontCreateCopyWithAttributes(ct_font_, (double)paint.size, nullptr, desc);
            CFRelease(desc);
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

    const void* GetTable(uint32_t tag, size_t* size,
                         minikin::MinikinDestroyFunc* destroy) override {
        hb_font_t* hb_font = hb_coretext_font_create(ct_font_);
        hb_face_t* hb_face = hb_font_get_face(hb_font);
        hb_blob_t* blob = hb_face_reference_table(hb_face, tag);

        hb_font_destroy(hb_font);
        return hb_blob_get_data(blob, reinterpret_cast<unsigned int*>(size));
    }

    size_t GetFontSize() const override { return CTFontGetSize(ct_font_); }

    bool Render(uint32_t glyph_id, const minikin::MinikinPaint& paint,
                minikin::GlyphBitmap* result) override {
        return false;
    }

    std::string GetFamilyName() override {
        CFStringRef cf_string = CTFontCopyFamilyName(ct_font_);
        char buffer[50] = {0};
        CFStringGetCString(cf_string, buffer, 50, kCFStringEncodingUTF8);
        std::string family_name = buffer;
        CFRelease(cf_string);
        return family_name;
    }

    void GetMetrics(FontMetrics* metrics, float size) override {
        CTFontRef font = ct_font_;
        if (size == CTFontGetSize(ct_font_)) {
            CFRetain(font);
        } else {
            auto desc = create_descriptor(typeface_->GetFamilyName(), typeface_->GetStyle());
            font = CTFontCreateCopyWithAttributes(ct_font_, size, nullptr, desc);
            CFRelease(desc);
        }

        metrics->ascent = CTFontGetAscent(font);
        metrics->descent = CTFontGetDescent(font);
        metrics->leading = CTFontGetLeading(font);
        CFRelease(font);
    }

private:
    CTFontRef ct_font_{};
};

std::shared_ptr<TypefaceFont> CTFontManager::MakeFont(const std::shared_ptr<Typeface>& typeface) {
    return std::make_shared<CTTypefaceFont>(std::dynamic_pointer_cast<CTTypeface>(typeface));
}

} // namespace txt
//
// Created by TangRuiwen on 2020/9/30.
//

#ifndef MINIKIN_INCLUDE_TXT_FONTCOLLECTION_H_
#define MINIKIN_INCLUDE_TXT_FONTCOLLECTION_H_

#include <minikin/FontCollection.h>

#include <memory>
#include <unordered_map>

namespace txt {
class FontManager;
class FontCollection final : public std::enable_shared_from_this<FontCollection> {
public:
    class TxtFallbackFontProvider : public minikin::FontCollection::FallbackFontProvider {
    public:
        explicit TxtFallbackFontProvider(const std::shared_ptr<FontCollection>& font_collection)
              : font_collection_(font_collection) {}

        std::shared_ptr<minikin::FontFamily> MatchFallbackFont(uint32_t ch,
                                                               const std::string& locale) override;

    private:
        std::weak_ptr<FontCollection> font_collection_;
    };

    ~FontCollection() = default;
    static std::shared_ptr<FontCollection> GetFontCollection();

    void SetupDefaultFontManager();

    std::shared_ptr<minikin::FontCollection> GetMinikinFontCollectionForFamilies(
            const std::vector<std::string>& font_families, const std::string& local);

    // Provides a FontFamily that contains glyphs for ch. This caches previously
    // matched fonts. Also see FontCollection::DoMatchFallbackFont.
    std::shared_ptr<minikin::FontFamily> MatchFallbackFont(uint32_t ch, const std::string& local);

    // Do not provide alternative fonts that can match characters which are
    // missing from the requested font family.
    void DisableFontFallback();

    // Remove all entries in the font family cache.
    void ClearFontFamilyCache();

private:
    FontCollection();

    std::shared_ptr<minikin::FontFamily> DoMatchFallbackFont(uint32_t ch,
                                                             const std::string& locale);

    std::shared_ptr<minikin::FontFamily> FindFontFamilyInManagers(const std::string& family_name);

    std::shared_ptr<minikin::FontFamily> GetFallbackFontFamily(
            const std::shared_ptr<FontManager>& manager, const std::string& family_name);

    std::shared_ptr<minikin::FontFamily> CreateMinikinFontFamily(
            const std::shared_ptr<FontManager>& manager, const std::string& family_name);
    struct FamilyKey {
        std::string font_families;
        std::string locale;

        FamilyKey(const std::vector<std::string>& families, const std::string& loc);

        bool operator==(const FamilyKey& key) const;

        struct Hasher {
            size_t operator()(const FamilyKey& key) const;
        };
    };

    std::unordered_map<FamilyKey, std::shared_ptr<minikin::FontCollection>, FamilyKey::Hasher>
            font_collections_cache_;

    std::shared_ptr<FontManager> default_font_manager_;
    // Cache that stores the results of MatchFallbackFont to ensure lag-free emoji
    // font fallback matching.
    std::unordered_map<uint32_t, std::shared_ptr<minikin::FontFamily>> fallback_match_cache_;
    std::unordered_map<std::string, std::shared_ptr<minikin::FontFamily>> fallback_fonts_;
    std::unordered_map<std::string, std::vector<std::string>> fallback_fonts_for_locale_;
    bool enable_font_fallback_;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_FONTCOLLECTION_H_

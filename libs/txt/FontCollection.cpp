//
// Created by TangRuiwen on 2020/9/30.
//

#include <txt/FontCollection.h>
#include <txt/Platform.h>

#include <algorithm>
#include <sstream>

namespace txt {

namespace {
const std::shared_ptr<android::FontFamily> g_null_family;
}

FontCollection::FamilyKey::FamilyKey(const std::vector<std::string>& families,
                                     const std::string& loc) {
    locale = loc;
    std::stringstream stream;
    std::for_each(families.begin(), families.end(),
                  [&stream](const std::string& str) { stream << str << ','; });
    font_families = stream.str();
}

bool FontCollection::FamilyKey::operator==(const FamilyKey& other) const {
    return font_families == other.font_families && locale == other.locale;
}

size_t FontCollection::FamilyKey::Hasher::operator()(const FamilyKey& key) const {
    return std::hash<std::string>()(key.font_families) ^ std::hash<std::string>()(key.locale);
}

FontCollection::FontCollection() : enable_font_fallback_(false) {}

void FontCollection::DisableFontFallback() {
    enable_font_fallback_ = false;
}

std::shared_ptr<android::FontCollection> FontCollection::GetMinikinFontCollectionForFamilies(
        const std::vector<std::string>& font_families, const std::string& local) {
    FamilyKey family_key{font_families, local};
    auto cached = font_collections_cache_.find(family_key);
    if (cached != font_collections_cache_.end()) {
        return cached->second;
    }

    std::vector<std::shared_ptr<android::FontFamily>> minikin_families;

    // search for all user provided font families.
    for (size_t fallback_index = 0; fallback_index < font_families.size(); fallback_index++) {
        auto minikin_family = FindFontFamilyInManagers(font_families[fallback_index]);
        if (minikin_family) {
            minikin_families.emplace_back(minikin_family);
        }
    }

    // search for default font family if no user font families were found.
    if (minikin_families.empty()) {
        const auto default_font_families = Platform::GetDefaultFontFamilies();
        for (const auto& family : default_font_families) {
            auto minikin_family = FindFontFamilyInManagers(family);
            if (minikin_family) {
                minikin_families.emplace_back(minikin_family);
                break;
            }
        }
    }

    // Default font family also not found. We fail to get a FontCollection.
    if (minikin_families.empty()) {
        font_collections_cache_[family_key] = nullptr;
        return nullptr;
    }

    if (enable_font_fallback_) {
        for (const auto& fallback_family : fallback_fonts_for_locale_[local]) {
            auto it = fallback_fonts_.find(fallback_family);
            if (it != fallback_fonts_.end()) {
                minikin_families.emplace_back(it->second);
            }
        }
    }
}

} // namespace txt
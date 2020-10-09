//
// Created by TangRuiwen on 2020/10/8.
//

#include <txt/Typeface.h>
#include <txt/TypefaceFontAssetProvider.h>

namespace txt {

static char kStyleKey = 0xFC;
static char kFamilyNameKey = 0xFE;

uint32_t TypefaceFontStyleSet::Count() const {
    return typefaces_.size();
}

std::string TypefaceFontStyleSet::GetStyle(uint32_t index, FontStyle* style) {
    if (index >= typefaces_.size()) {
        return "";
    }

    *style = typefaces_[index]->GetStyle();

    return "";
}

void TypefaceFontStyleSet::RegisterTypefaceFace(const std::shared_ptr<Typeface>& typeface) {
    if (!typeface) {
        return;
    }

    typefaces_.emplace_back(typeface);
}

std::shared_ptr<Typeface> TypefaceFontStyleSet::CreateTypeface(uint32_t index) {
    if (index >= typefaces_.size()) {
        return nullptr;
    }

    return typefaces_[index];
}

std::shared_ptr<Typeface> TypefaceFontStyleSet::MatchStyle(const FontStyle& style) {
    return MatchStyleCSS3(style);
}

void TypefaceFontAssetProvider::RegisterTypeface(const std::shared_ptr<Typeface>& typeface,
                                                 const std::string& family_name_alias) {
    if (family_name_alias.empty()) {
        return;
    }

    std::string canonical_name = CanonicalFamilyName(family_name_alias);
    auto family_it = registered_families_.find(canonical_name);
    if (family_it == registered_families_.end()) {
        family_names_.emplace_back(family_name_alias);
        auto value = std::make_pair(canonical_name, std::make_shared<TypefaceFontStyleSet>());
        family_it = registered_families_.emplace(value).first;
    }
    family_it->second->RegisterTypefaceFace(typeface);
}

void TypefaceFontAssetProvider::RegisterTypeface(const std::shared_ptr<Typeface>& typeface) {
    if (!typeface) {
        return;
    }

    std::string family_name = typeface->GetFamilyName();
    RegisterTypeface(typeface, family_name);
}

size_t TypefaceFontAssetProvider::GetFamilyCount() const {
    return family_names_.size();
}

std::string TypefaceFontAssetProvider::GetFamilyName(size_t index) const {
    return family_names_[index];
}

FontStyleSet* TypefaceFontAssetProvider::MatchFamily(const std::string& family_name) {
    auto found = registered_families_.find(CanonicalFamilyName(family_name));
    if (found == registered_families_.end()) {
        return nullptr;
    }

    return found->second.get();
}

} // namespace txt
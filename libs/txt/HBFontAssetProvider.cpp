//
// Created by TangRuiwen on 2020/10/8.
//

#include <txt/HBFontAssetProvider.h>

namespace txt {

static char kStyleKey = 0xFC;
static char kFamilyNameKey = 0xFE;

HBFontStyleSet::~HBFontStyleSet() {
    for (auto hb_face : hb_faces_) {
        hb_face_destroy(hb_face);
    }
}

uint32_t HBFontStyleSet::Count() const {
    return hb_faces_.size();
}

std::string HBFontStyleSet::GetStyle(uint32_t index, FontStyle* style) {
    if (index >= hb_faces_.size()) {
        return "";
    }
    hb_user_data_key_t key{kStyleKey};
    FontStyle* font_style =
            static_cast<FontStyle*>(hb_face_get_user_data(hb_faces_[index], std::addressof(key)));
    if (font_style != nullptr) {
        *style = *font_style;
    }

    return "";
}

hb_face_t* HBFontStyleSet::CreateTypeface(uint32_t index) {
    if (index >= hb_faces_.size()) {
        return nullptr;
    }

    return hb_face_reference(hb_faces_[index]);
}

hb_face_t* HBFontStyleSet::MatchStyle(const FontStyle& style) {
    return MatchStyleCSS3(style);
}

void HBFontStyleSet::RegisterHBFace(hb_face_t* hb_face, const FontStyle& style) {
    if (hb_face == nullptr) {
        return;
    }

    FontStyle* copy_style = new FontStyle(style);
    hb_user_data_key_t key{kStyleKey};
    hb_bool_t success = hb_face_set_user_data(
            hb_face, std::addressof(key), copy_style,
            [](void* data) {
                if (data) {
                    delete static_cast<FontStyle*>(data);
                }
            },
            true);
    if (!success) {
        return;
    }

    hb_faces_.emplace_back(hb_face);
}

void HBFontAssetProvider::RegisterHBFace(hb_face_t* face, const FontStyle& style) {
    if (!face) {
        return;
    }

    hb_user_data_key_t key{kFamilyNameKey};
    std::string name = (char*)hb_face_get_user_data(face, std::addressof(key));
    RegisterHBFace(face, style, name);
}

void HBFontAssetProvider::RegisterHBFace(hb_face_t* face, const FontStyle& style,
                                         std::string family_name_alias) {
    if (family_name_alias.empty()) {
        return;
    }

    std::string canonical_name = CanonicalFamilyName(family_name_alias);
    auto family_it = registered_families_.find(canonical_name);
    if (family_it == registered_families_.end()) {
        family_names_.emplace_back(family_name_alias);

        auto value = std::make_pair(canonical_name, std::make_shared<HBFontStyleSet>());
        family_it = registered_families_.emplace(value).first;
    }
    family_it->second->RegisterHBFace(face, style);
}

size_t HBFontAssetProvider::GetFamilyCount() const {
    return family_names_.size();
}

std::string HBFontAssetProvider::GetFamilyName(size_t index) const {
    return family_names_[index];
}

FontStyleSet* HBFontAssetProvider::MatchFamily(const std::string& family_name) {
    auto found = registered_families_.find(CanonicalFamilyName(family_name));
    if (found == registered_families_.end()) {
        return nullptr;
    }

    return found->second.get();
}

} // namespace txt
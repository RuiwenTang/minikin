//
// Created by TangRuiwen on 2020/10/8.
//

#ifndef MINIKIN_INCLUDE_TXT_HBFONTASSETPROVIDER_H_
#define MINIKIN_INCLUDE_TXT_HBFONTASSETPROVIDER_H_

#include <txt/FontAssetProvider.h>

#include <unordered_map>
#include <vector>

namespace txt {

class HBFontStyleSet : public FontStyleSet {
public:
    HBFontStyleSet();
    ~HBFontStyleSet() override;

    uint32_t Count() const override;
    std::string GetStyle(FontStyle* style) override;
    hb_face_t* CreateTypeface(uint32_t index) override;
    hb_face_t* MatchStyle(const FontStyle& style) override;

    void RegisterHBFace(hb_face_t* hb_face);

private:
    std::vector<hb_face_t*> hb_faces_;
};

class HBFontAssetProvider : public FontAssetProvider {
public:
    HBFontAssetProvider();
    ~HBFontAssetProvider() override;

    void RegisterHBFace(hb_face_t* face);
    void RegisterHBFace(hb_face_t* face, std::string family_name_alias);

    size_t GetFamilyCount() const override;
    std::string GetFamilyName(size_t index) const override;
    FontStyleSet* MatchFamily(const std::string& family_name) override;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_HBFONTASSETPROVIDER_H_

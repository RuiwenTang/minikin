//
// Created by TangRuiwen on 2020/10/07.
//

#ifndef MINIKIN_INCLUDE_TXT_FONTASSETPROVIDER_H_
#define MINIKIN_INCLUDE_TXT_FONTASSETPROVIDER_H_

#include <string>

namespace txt {

class FontAssetProvider {
public:
    virtual ~FontAssetProvider() = default;
    virtual size_t GetFamilyCount() const = 0;
    virtual std::string GetFamilyName(int index) const = 0;
    virtual SkFontStyleSet* MatchFamily(const std::string& family_name) = 0;

protected:
    static std::string CanonicalFamilyName(std::string family_name);
};
} // namespace txt

#endif
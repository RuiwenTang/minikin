//
// Created by TangRuiwen on 2020/10/8.
//

#ifndef MINIKIN_INCLUDE_TXT_TYPEFACEFONTASSETPROVIDER_H_
#define MINIKIN_INCLUDE_TXT_TYPEFACEFONTASSETPROVIDER_H_

#include <txt/FontAssetProvider.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace txt {

class Typeface;
class TypefaceFontStyleSet : public FontStyleSet {
public:
    TypefaceFontStyleSet() = default;
    ~TypefaceFontStyleSet() override = default;

    uint32_t Count() const override;
    std::string GetStyle(uint32_t index, FontStyle* style) override;
    std::shared_ptr<Typeface> CreateTypeface(uint32_t index) override;
    std::shared_ptr<Typeface> MatchStyle(const FontStyle& style) override;

    void RegisterTypefaceFace(const std::shared_ptr<Typeface>& typeface);

private:
    std::vector<std::shared_ptr<Typeface>> typefaces_;
};

class TypefaceFontAssetProvider : public FontAssetProvider {
public:
    TypefaceFontAssetProvider() = default;
    ~TypefaceFontAssetProvider() override = default;

    void RegisterTypeface(const std::shared_ptr<Typeface>& typeface);
    void RegisterTypeface(const std::shared_ptr<Typeface>& typeface, const std::string& family_name_alias);

    size_t GetFamilyCount() const override;
    std::string GetFamilyName(size_t index) const override;
    FontStyleSet* MatchFamily(const std::string& family_name) override;

private:
    std::unordered_map<std::string, std::shared_ptr<TypefaceFontStyleSet>> registered_families_;
    std::vector<std::string> family_names_;
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_TYPEFACEFONTASSETPROVIDER_H_

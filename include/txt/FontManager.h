//
// Created by TangRuiwen on 2020/10/9.
//

#ifndef MINIKIN_INCLUDE_TXT_FONTMANAGER_H_
#define MINIKIN_INCLUDE_TXT_FONTMANAGER_H_

#include <txt/FontStyle.h>

#include <memory>
#include <string>

namespace txt {

class Typeface;
class TypefaceFont;

class FontManager {
public:
    virtual ~FontManager() = default;
    virtual int CountFamilies() const = 0;
    virtual std::shared_ptr<FontStyleSet> MatchFamily(const std::string& family_name) const = 0;
    virtual std::shared_ptr<Typeface> MatchFamilyStyle(const std::string& family_name,
                                                       const FontStyle& style) const = 0;
    virtual std::shared_ptr<Typeface> MatchFamilyStyleCharacter(const std::string& family_name,
                                                                const FontStyle& style,
                                                                const std::string& locale,
                                                                uint32_t character) const = 0;
    virtual std::shared_ptr<Typeface> MakeFromFile(const std::string& path) = 0;
    virtual std::shared_ptr<Typeface> MakeFromName(const std::string& family_name,
                                                   const FontStyle& style) = 0;

    virtual std::shared_ptr<TypefaceFont> MakeFont(const std::shared_ptr<Typeface>& typeface) = 0;
    // Return the default FontManager
    static std::shared_ptr<FontManager> Default();
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_FONTMANAGER_H_

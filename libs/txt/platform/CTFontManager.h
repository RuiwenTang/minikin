//
// Created by TangRuiwen on 2020/10/9.
//

#ifndef MINIKIN_LIBS_TXT_PLATFORM_CTFONTMANAGER_H_
#define MINIKIN_LIBS_TXT_PLATFORM_CTFONTMANAGER_H_

#include <CoreText/CoreText.h>
#include <txt/FontManager.h>

namespace txt {

class CTFontManager : public FontManager {
public:
    CTFontManager();
    ~CTFontManager() override;
    CFStringRef GetFamilyNameAt(uint32_t index) const {
        assert(index < count_);
        return static_cast<CFStringRef>(CFArrayGetValueAtIndex(names_, index));
    }

    int CountFamilies() const override { return count_; }
    std::shared_ptr<FontStyleSet> MatchFamily(const std::string& family_name) const override;
    std::shared_ptr<Typeface> MatchFamilyStyle(const std::string& family_name,
                                               const FontStyle& style) const override;
    std::shared_ptr<Typeface> MatchFamilyStyleCharacter(const std::string& family_name,
                                                        const FontStyle& style,
                                                        const std::string& locale,
                                                        uint32_t character) const override;
    std::shared_ptr<Typeface> MakeFromFile(const std::string& path) override;
    std::shared_ptr<Typeface> MakeFromName(const std::string& family_name,
                                           const FontStyle& style) override;

    std::shared_ptr<TypefaceFont> MakeFont(const std::shared_ptr<Typeface>& typeface) override;

    static std::shared_ptr<FontStyleSet> CreateSet(CFStringRef cf_family_name);

    static CFArrayRef CopyAvailableFontFamilyNames();

private:
    CFArrayRef names_;
    uint32_t count_;
};

} // namespace txt

#endif // MINIKIN_LIBS_TXT_PLATFORM_CTFONTMANAGER_H_

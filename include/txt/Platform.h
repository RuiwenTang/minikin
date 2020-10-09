//
// Created by TangRuiwen on 2020/9/30.
//

#ifndef MINIKIN_INCLUDE_TXT_PLATFORM_HPP_
#define MINIKIN_INCLUDE_TXT_PLATFORM_HPP_

#include <memory>
#include <string>
#include <vector>

namespace txt {

class FontCollection;
class FontManager;
class Platform {
public:
    static std::vector<std::string> GetDefaultFontFamilies();
    static std::shared_ptr<FontManager> GetDefaultFontManager();
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_PLATFORM_HPP_

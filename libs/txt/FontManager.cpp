//
// Created by TangRuiwen on 2020/10/9.
//

#include <txt/FontManager.h>
#include <txt/Platform.h>

namespace txt {

std::shared_ptr<FontManager> FontManager::Default() {
    return Platform::GetDefaultFontManager();
}

} // namespace txt
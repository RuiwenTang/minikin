//
// Created by TangRuiwen on 2020/10/07.
//

#include <txt/FontAssetProvider.h>

#include <algorithm>

namespace txt {

std::string FontAssetProvider::CanonicalFamilyName(std::string family_name) {
    std::string result(family_name.length(), 0);

    // Convert ASCII characters to lower case.
    std::transform(family_name.begin(), family_name.end(), result.begin(),
                   [](char c) { return (c & 0x80) ? c : ::tolower(c); });

    return result;
}

} // namespace txt
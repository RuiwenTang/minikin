#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <txt/Platform.h>

namespace txt {

std::vector<std::string> Platform::GetDefaultFontFamilies() {
    if (@available(iOS 11, *)) {
        CTFontRef ct_font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 14, 0);
        CFStringRef cf_name = CTFontCopyFamilyName(ct_font);
        char buffer[100] = {0};
        CFStringGetCString(cf_name, buffer, 100, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
        CFRelease(cf_name);
        CFRelease(ct_font);
        return {std::string(buffer)};
    } else {
        return {"Helvetica"};
    }
}

}
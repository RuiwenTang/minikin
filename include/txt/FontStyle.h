//
// Created by TangRuiwen on 2020/10/7.
//

#ifndef MINIKIN_INCLUDE_TXT_FONTSTYLE_H_
#define MINIKIN_INCLUDE_TXT_FONTSTYLE_H_

#include <hb.h>

#include <cstdint>
#include <string>

namespace txt {

class FontStyle {
public:
    enum Weight {
        kInvisible_Weight = 0,
        kThin_Weight = 100,
        kExtraLight_Weight = 200,
        kLight_Weight = 300,
        kNormal_Weight = 400,
        kMedium_Weight = 500,
        kSemiBold_Weight = 600,
        kBold_Weight = 700,
        kExtraBold_Weight = 800,
        kBlack_Weight = 900,
        kExtraBlack_Weight = 1000,
    };

    enum Width {
        kUltraCondensed_Width = 1,
        kExtraCondensed_Width = 2,
        kCondensed_Width = 3,
        kSemiCondensed_Width = 4,
        kNormal_Width = 5,
        kSemiExpanded_Width = 6,
        kExpanded_Width = 7,
        kExtraExpanded_Width = 8,
        kUltraExpanded_Width = 9,
    };

    enum Slant {
        kUpright_Slant,
        kItalic_Slant,
        kOblique_Slant,
    };

    constexpr FontStyle(Weight weight, Width width, Slant slant)
          : weight_(weight), width_(width), slant_(slant) {}

    constexpr FontStyle()
          : weight_{Weight::kNormal_Weight},
            width_{Width::kNormal_Width},
            slant_{Slant::kUpright_Slant} {}

    bool operator==(const FontStyle& rhs) const {
        return weight_ == rhs.weight_ && width_ == rhs.width_ && slant_ == rhs.slant_;
    }
    bool operator!=(const FontStyle& rhs) const { return !(rhs == *this); }

    Weight GetWeight() const { return weight_; }
    Width GetWidth() const { return width_; }
    Slant GetSlant() const { return slant_; }

    static constexpr FontStyle Normal() {
        return {Weight::kNormal_Weight, Width::kNormal_Width, Slant::kUpright_Slant};
    }

    static constexpr FontStyle Bold() {
        return {Weight::kBold_Weight, Width::kNormal_Width, Slant::kUpright_Slant};
    }

    static constexpr FontStyle Italic() {
        return {Weight::kNormal_Weight, Width::kNormal_Width, Slant::kItalic_Slant};
    }

    static constexpr FontStyle BoldItalic() {
        return {Weight::kBold_Weight, Width::kNormal_Width, Slant::kItalic_Slant};
    }

private:
    Weight weight_;
    Width width_;
    Slant slant_;
};

class FontStyleSet {
public:
    virtual ~FontStyleSet() = default;

    virtual uint32_t Count() const = 0;
    virtual std::string GetStyle(uint32_t index, FontStyle* style) = 0;
    virtual hb_face_t* CreateTypeface(uint32_t index) = 0;
    virtual hb_face_t* MatchStyle(const FontStyle& style) = 0;

    static FontStyleSet* CreateEmpty();

protected:
    hb_face_t* MatchStyleCSS3(const FontStyle& style);
};

} // namespace txt

#endif // MINIKIN_INCLUDE_TXT_FONTSTYLE_H_

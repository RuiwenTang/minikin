//
// Created by TangRuiwen on 2020/10/7.
//

#include <txt/FontStyle.h>

namespace txt {

struct Score {
    uint32_t score;
    uint32_t index;
    Score& operator+=(uint32_t rhs) {
        this->score += rhs;
        return *this;
    }
    Score& operator<<=(uint32_t rhs) {
        this->score = this->score << rhs;
        return *this;
    }
    bool operator<(const Score& that) const { return this->score < that.score; }
};

/**
 * Width has the greatest priority.
 * If the value of pattern.width is 5 (normal) or less,
 *    narrower width values are checked first, then wider values.
 * If the value of pattern.width is greater than 5 (normal),
 *    wider values are checked first, followed by narrower values.
 *
 * Italic/Oblique has the next highest priority.
 * If italic requested and there is some italic font, use it.
 * If oblique requested and there is some oblique font, use it.
 * If italic requested and there is some oblique font, use it.
 * If oblique requested and there is some italic font, use it.
 *
 * Exact match.
 * If pattern.weight < 400, weights below pattern.weight are checked
 *   in descending order followed by weights above pattern.weight
 *   in ascending order until a match is found.
 * If pattern.weight > 500, weights above pattern.weight are checked
 *   in ascending order followed by weights below pattern.weight
 *   in descending order until a match is found.
 * If pattern.weight is 400, 500 is checked first
 *   and then the rule for pattern.weight < 400 is used.
 * If pattern.weight is 500, 400 is checked first
 *   and then the rule for pattern.weight < 400 is used.
 */
hb_face_t* FontStyleSet::MatchStyleCSS3(const FontStyle& style) {
    uint32_t count = this->Count();
    if (count == 0) {
        return nullptr;
    }

    Score max_score{0, 0};
    for (uint32_t i = 0; i < count; i++) {
        FontStyle current;
        this->GetStyle(std::addressof(current));
        Score current_score{0, i};

        // CSS stretch / FontStyle::Width
        // Takes priority over everything else.
        if (style.GetWidth() <= FontStyle::Width::kNormal_Width) {
            if (current.GetWidth() <= style.GetWidth()) {
                current_score += 10 - style.GetWidth() + current.GetWidth();
            } else {
                current_score += 10 - current.GetWidth();
            }
        } else {
            if (current.GetWidth() > style.GetWidth()) {
                current_score += 10 + style.GetWidth() - current.GetWidth();
            } else {
                current_score += current.GetWidth();
            }
        }
        current_score <<= 8;

        // CSS style (normal, italic, oblique) / FontStyle::Slant (upright, italic, oblique)
        // Takes priority over all valid weights.
        static_assert(FontStyle::kUpright_Slant == 0 && FontStyle::kItalic_Slant == 1 &&
                              FontStyle::kOblique_Slant == 2,
                      "FontStyle::Slant values not as required.");
        assert(0 <= style.GetSlant() && style.GetSlant() <= 2 && 0 <= current.GetSlant() &&
               current.GetSlant() <= 2);

        static const int score[3][3] = {
                /*               Upright Italic Oblique  [current]*/
                /*   Upright */ {3, 1, 2},
                /*   Italic  */ {1, 3, 2},
                /*   Oblique */ {1, 2, 3},
                /* [pattern] */
        };

        current_score += score[style.GetSlant()][current.GetSlant()];
        current_score <<= 8;
        // Synthetics (weight, style) [no stretch synthetic?]

        // CSS weight / FontStyle::Weight
        // The 'closer' to the target weight, the higher the score.
        // 1000 is the 'heaviest' recognized weight
        if (style.GetWeight() == current.GetWeight()) {
            current_score += 1000;
        } else if (style.GetWeight() < 400) { // less than 400 prefer lighter weights
            if (current.GetWeight() <= style.GetWeight()) {
                current_score += 1000 - style.GetWeight() + current.GetWeight();
            } else {
                current_score += 1000 - current.GetWeight();
            }
        } else if (style.GetWeight() <= 500) {
            // between 400 and 500 prefer heavier up to 500, then lighter weights
            if (current.GetWeight() >= style.GetWeight() && current.GetWeight() <= 500) {
                current_score += 1000 + style.GetWeight() - current.GetWeight();
            } else if (current.GetWeight() <= style.GetWeight()) {
                current_score += 500 + current.GetWeight();
            } else {
                current_score += 1000 - current.GetWeight();
            }
        } else if (style.GetWeight() > 500) { // greater than 500 prefer heavier weights
            if (current.GetWeight() > style.GetWeight()) {
                current_score += 1000 + style.GetWeight() - current.GetWeight();
            } else {
                current_score += current.GetWeight();
            }
        }

        if (max_score < current_score) {
            max_score = current_score;
        }
    }
    return this->CreateTypeface(max_score.index);
}

} // namespace txt
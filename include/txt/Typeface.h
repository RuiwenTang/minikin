//
// Created by TangRuiwen on 2020/10/9.
//

#ifndef MINIKIN_LIBS_TXT_TYPEFACE_HPP_
#define MINIKIN_LIBS_TXT_TYPEFACE_HPP_

#include <minikin/MinikinFont.h>
#include <txt/FontStyle.h>

#include <memory>
#include <string>

namespace txt {

class Typeface : public std::enable_shared_from_this<Typeface> {
public:
    explicit Typeface(const FontStyle& style);
    virtual ~Typeface() = default;

    FontStyle GetStyle() const { return style_; }

    bool IsBold() const { return style_.GetWeight() >= FontStyle::kSemiBold_Weight; }
    bool IsItalic() const { return style_.GetSlant() != FontStyle::kUpright_Slant; }

    uint32_t UniqueID() const { return unique_id_; }

    virtual std::string GetFamilyName() = 0;

private:
    static uint32_t NextID();

private:
    FontStyle style_;
    uint32_t unique_id_;
};

class TypefaceFont : public minikin::MinikinFont {
public:
    explicit TypefaceFont(const std::shared_ptr<Typeface>& typeface)
          : minikin::MinikinFont(typeface->UniqueID()), typeface_(typeface) {}
    ~TypefaceFont() override = default;

    const std::shared_ptr<Typeface>& GetTypeface() const { return typeface_; }

    static std::shared_ptr<TypefaceFont> MakeFont(const std::shared_ptr<Typeface>& typeface);

protected:
    std::shared_ptr<Typeface> typeface_;
};

} // namespace txt

#endif // MINIKIN_LIBS_TXT_TYPEFACE_HPP_

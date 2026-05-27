#pragma once

#include <string>
#include <array>
#include <unordered_map>

namespace randomizer {
    class Text {
    public:
        enum Language {
            ENGLISH = 0,
            LANGUAGE_MAX
        };

        enum Type
        {
            STANDARD = 0,
            PRETTY,
            CRYPTIC,
            TYPE_MAX
        };

        enum Color
        {
            RAW = 0,
            NONE,
            RED,
            GREEN,
            BLUE,
            YELLOW,
            CYAN,
            MAGENTA,
            GRAY,
            ORANGE,
        };

        enum Gender
        {
            NUETRAL = 0,
            MASCULINE,
            FEMININE,
            GENDER_MAX,
        };

        enum Plurality
        {
            SINGULAR = 0,
            PLURAL,
            PLURALITY_MAX,
        };

        std::array<std::string, LANGUAGE_MAX> mText{};
        std::array<Gender, LANGUAGE_MAX> mGender{};
        std::array<Plurality, LANGUAGE_MAX> mPlurality{};
    };

    inline constexpr std::array supported_languages = {
        Text::ENGLISH,
    };

    // std::u16string apply_name_color(std::u16string str, const Color& color);
    // std::u16string word_wrap_string(const std::u16string& string, const size_t& max_line_len); //IMPROVEMENT: use font data to do this "properly"
    // std::string    pad_str_4_lines(const std::string& string);
    // std::u16string pad_str_4_lines(const std::u16string& string);

    Text::Gender string_to_gender(const std::string& str);
    Text::Plurality string_to_plurality(const std::string& str);

    // Retrieval of Text objects keyed by name and type (standard, pretty, cryptic)
    using TextDatabase = std::unordered_map<std::string, std::array<Text, Text::TYPE_MAX>>;

    const TextDatabase& getTextDatabase();

    const std::string& getTextStr(const std::string& name, Text::Type type = Text::STANDARD, Text::Language language = Text::ENGLISH);

    // Replaces the message codes in the string with the ingame hex equivalents
    void applyMessageCodes(std::string&);
}; // namespace Text

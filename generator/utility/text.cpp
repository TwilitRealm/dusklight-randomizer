#include "text.hpp"

#include "yaml.hpp"

#include <fmt/format.h>

#include <unordered_map>


namespace randomizer {

    // std::array<std::string, 3> supported_languages = {"English", "Spanish", "French"};
    //
    // static std::unordered_map<Text::Color, std::u16string> nameToColor = {
    //     {Text::Color::NONE,    TEXT_COLOR_DEFAULT},
    //     {Text::Color::RED,     TEXT_COLOR_RED},
    //     {Text::Color::GREEN,   TEXT_COLOR_GREEN},
    //     {Text::Color::BLUE,    TEXT_COLOR_BLUE},
    //     {Text::Color::YELLOW,  TEXT_COLOR_YELLOW},
    //     {Text::Color::CYAN,    TEXT_COLOR_CYAN},
    //     {Text::Color::MAGENTA, TEXT_COLOR_MAGENTA},
    //     {Text::Color::GRAY,    TEXT_COLOR_GRAY},
    //     {Text::Color::ORANGE,  TEXT_COLOR_ORANGE},
    // };
    //
    // std::u16string apply_name_color(std::u16string str, const Color& color)
    // {
    //     // Return the raw text (bars included)
    //     if (color == Color::RAW)
    //     {
    //         return str;
    //     }
    //     // If there are no '|'s then just return with the color surrounding the whole string
    //     if (str.find('|') == std::string::npos)
    //     {
    //         auto textColor = nameToColor[color];
    //         return textColor + str + TEXT_COLOR_DEFAULT;
    //     }
    //
    //     // Alternate between the text color and default incase there are multiple
    //     // pairs of bars
    //     auto textColor = nameToColor[color];
    //     bool insertColor = false;
    //     for (size_t pos = 0; pos < str.length(); pos++)
    //     {
    //         if (str[pos] == '|')
    //         {
    //             insertColor = !insertColor;
    //             str.erase(pos, 1);
    //             str.insert(pos, insertColor ? textColor : TEXT_COLOR_DEFAULT);
    //         }
    //     }
    //
    //     return str;
    // }
    //
    // std::u16string word_wrap_string(const std::u16string& string, const size_t& max_line_len) {
    //     size_t index_in_str = 0;
    //     std::u16string wordwrapped_str;
    //     std::u16string current_word;
    //     size_t curr_word_len = 0;
    //     size_t len_curr_line = 0;
    //
    //     while (index_in_str < string.length()) { //length is weird because its utf-16
    //         char16_t character = string[index_in_str];
    //
    //         if (character == u'\x0E') { //need to parse the commands, only implementing a few necessary ones for now (will break with other commands)
    //             std::u16string substr;
    //             size_t code_len = 0;
    //             if (string[index_in_str + 1] == u'\x00') {
    //                 if (string[index_in_str + 2] == u'\x03') { //color command
    //                     if (string[index_in_str + 4] == u'\xFFFF') { //text color white, weird length
    //                     code_len = 10;
    //                     }
    //                     else {
    //                     code_len = 5;
    //                     }
    //                 }
    //             }
    //             else if (string[index_in_str + 1] == u'\x01') { //all implemented commands in this group have length 4
    //                 code_len = 4;
    //             }
    //             else if (string[index_in_str + 1] == u'\x02') { //all implemented commands in this group have length 4
    //                 code_len = 4;
    //             }
    //             else if (string[index_in_str + 1] == u'\x03') { //all implemented commands in this group have length 4
    //                 code_len = 4;
    //             }
    //             else if (string[index_in_str + 1] == u'\x04') { //all implemented commands in this group have length 4. Only used for Ho Ho sound
    //                 code_len = 4;
    //             }
    //
    //             substr = string.substr(index_in_str, code_len);
    //             current_word += substr;
    //             index_in_str += code_len;
    //         }
    //         else if (character == u'\n') {
    //             wordwrapped_str += current_word;
    //             wordwrapped_str += character;
    //             len_curr_line = 0;
    //             current_word = u"";
    //             curr_word_len = 0;
    //             index_in_str += 1;
    //         }
    //         else if (character == u' ') {
    //             wordwrapped_str += current_word;
    //             wordwrapped_str += character;
    //             len_curr_line += curr_word_len + 1;
    //             current_word = u"";
    //             curr_word_len = 0;
    //             index_in_str += 1;
    //         }
    //         else {
    //             current_word += character;
    //             curr_word_len += 1;
    //             index_in_str += 1;
    //
    //             if (len_curr_line + curr_word_len > max_line_len) {
    //                 wordwrapped_str += u'\n';
    //                 len_curr_line = 0;
    //
    //                 if (curr_word_len > max_line_len) {
    //                     wordwrapped_str += current_word + u'\n';
    //                     current_word = u"";
    //                 }
    //             }
    //         }
    //     }
    //     wordwrapped_str += current_word;
    //
    //     return wordwrapped_str;
    // }
    //
    // std::string pad_str_4_lines(const std::string& string)
    // {
    //     std::vector<std::string> lines = randomizer::utility::str::Split(string, '\n');
    //
    //     unsigned int padding_lines_needed = (4 - lines.size() % 4) % 4;
    //     for (unsigned int i = 0; i < padding_lines_needed; i++)
    //     {
    //         lines.push_back("");
    //     }
    //
    //     return randomizer::utility::str::Merge(lines, '\n');
    // }
    //
    // std::u16string pad_str_4_lines(const std::u16string& string)
    // {
    //     std::vector<std::u16string> lines = randomizer::utility::str::Split(string, u'\n');
    //
    //     unsigned int padding_lines_needed = (4 - lines.size() % 4) % 4;
    //     for (unsigned int i = 0; i < padding_lines_needed; i++)
    //     {
    //         lines.push_back(u"");
    //     }
    //
    //     return randomizer::utility::str::erge(lines, u'\n');
    // }

    Text::Text(const std::string& str) {
        for (auto& text : mText) {
            text = str;
        }
    }

    void Text::Replace(const std::string& oldStr, const Text& replacementText, int count/* = 1*/) {
        for (size_t i = 0; i < mText.size(); ++i) {
            auto& curString = mText[i];
            for (int i = 0; i < count; ++i) {
                if (auto startPos = curString.find(oldStr); startPos != std::string::npos) {
                    curString.replace(startPos, oldStr.length(), replacementText.mText[i]);
                }
            }
        }
    }

    void Text::Replace(const std::string& oldStr, const std::string& replacementText, int count/* = 1*/) {
        for (size_t i = 0; i < mText.size(); ++i) {
            auto& curString = mText[i];
            for (int i = 0; i < count; ++i) {
                if (auto startPos = curString.find(oldStr); startPos != std::string::npos) {
                    curString.replace(startPos, oldStr.length(), replacementText);
                }
            }
        }
    }

    bool Text::Empty() const {
        for (auto& text : mText) {
            if (!text.empty()) {
                return false;
            }
        }
        return true;
    }

    Text& Text::operator+=(const Text& rhs) {
        for (size_t i = 0; i < mText.size(); ++i) {
            mText[i] += rhs.mText[i];
        }
        return *this;
    }

    Text& Text::operator+=(const std::string& rhs) {
        for (auto& text : mText) {
            text += rhs;
        }
        return *this;
    }

    Text operator+(Text lhs, const Text& rhs) {
        lhs += rhs;
        return lhs;
    }

    Text operator+(Text lhs, const std::string& rhs) {
        for (auto& text : lhs.mText) {
            text += rhs;
        }
        return lhs;
    }

    Text operator+(const std::string& lhs, const Text& rhs) {
        return Text(lhs) + rhs;
    }

    Text::Type stringToType(const std::string& str) {
        std::unordered_map<std::string, Text::Type> strToType = {
            {"Standard", Text::Type::STANDARD},
            {"Pretty", Text::Type::PRETTY},
            {"Cryptic", Text::Type::CRYPTIC},
        };

        if (strToType.contains(str))
        {
            return strToType.at(str);
        }

        throw std::runtime_error("Text type \"" + str + "\" is not recognized.");
    }

    Text::Language stringToLanguage(const std::string& str) {
        std::unordered_map<std::string, Text::Language> strToLanguage = {
            {"english",  Text::ENGLISH},
            {"spanish",  Text::SPANISH},
            {"french",   Text::FRENCH},
            {"german",   Text::GERMAN},
            {"italian",  Text::ITALIAN},
            {"japanese", Text::JAPANESE}
        };

        if (strToLanguage.contains(str))
        {
            return strToLanguage.at(str);
        }

        throw std::runtime_error("Language \"" + str + "\" is not recognized.");
    }


    std::string languageToString(Text::Language language) {
        switch (language) {
            case Text::ENGLISH:
                return "english";
            case Text::SPANISH:
                return "spanish";
            case Text::FRENCH:
                return "french";
            case Text::GERMAN:
                return "german";
            case Text::ITALIAN:
                return "italian";
            case Text::JAPANESE:
                return "japanese";
            default:
                return "unknown language enum";
        }
    }

    Text::Gender stringToGender(const std::string& str)
    {
        std::unordered_map<std::string, Text::Gender> strToGender = {
            {"Masculine", Text::Gender::MASCULINE},
            {"Feminine", Text::Gender::FEMININE}
        };

        if (strToGender.contains(str))
        {
            return strToGender.at(str);
        }

        return Text::Gender::NUETRAL;
    }

    Text::Plurality stringToPlurality(const std::string& str)
    {
        if (str == "Plural") return Text::Plurality::PLURAL;
        return Text::Plurality::SINGULAR;
    }

    std::string UTF8ToLatin1(const std::string& utf8Str) {
        std::string latin1Str;
        // The output string will be equal to or shorter than the UTF-8 string
        latin1Str.reserve(utf8Str.length());

        size_t read_pos = 0;
        size_t len = utf8Str.length();

        while (read_pos < len) {
            unsigned char c = utf8Str[read_pos];

            if (c < 0x80) {
                // Standard ASCII (0x00 - 0x7F)
                latin1Str.push_back(c);
                ++read_pos;
            }
            else if ((c & 0xE0) == 0xC0 && (read_pos + 1 < len)) {
                // Two-byte UTF-8 sequence (0xC0 - 0xDF)
                unsigned char next_byte = utf8Str[read_pos + 1];

                // Reconstruct the Latin-1 character value
                unsigned char latin1_char = ((c & 0x1F) << 6) | (next_byte & 0x3F);

                latin1Str.push_back(latin1_char);
                read_pos += 2;
            }
            else {
                // Multi-byte sequences out of Latin-1 range (or malformed bytes)
                throw std::runtime_error(fmt::format("Invalid bytes when converting to Latin1 with \"{}\"", utf8Str));
            }
        }

        return latin1Str;
    }

    static void LoadTextData(TextDatabase& tb) {
        struct LanguageEntry {
            std::string language;
            std::string languageData;
        };
        auto files = std::to_array<LanguageEntry>({
            {"english", GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/english.yaml")},
            {"spanish", GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/spanish.yaml")},
            {"french",  GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/french.yaml")},
            {"german",  GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/german.yaml")},
            {"italian", GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/italian.yaml")},
        });

        for (const auto& file : files) {
            auto language = stringToLanguage(file.language);
            auto textData = LOAD_EMBED_DATA(file.languageData);
            for (const auto& textNode : textData) {
                const auto& name = textNode.first.as<std::string>();
                for (const auto& typeNode : textNode.second) {
                    auto type = stringToType(typeNode.first.as<std::string>());
                    auto typeData = typeNode.second;
                    const auto& text = typeData["Text"].as<std::string>();
                    if (language != Text::JAPANESE) {
                        tb[name][type].mText[language] = UTF8ToLatin1(text);
                    } else {
                        // Probably have to handle Japanese another way at some point
                        tb[name][type].mText[language] = text;
                    }
                    if (typeData["Gender"]) {
                        tb[name][type].mGender[language] = stringToGender(typeData["Gender"].as<std::string>());
                    }
                    if (typeData["Plurality"]) {
                        tb[name][type].mPlurality[language] = stringToPlurality(typeData["Plurality"].as<std::string>());
                    }
                }
            }
        }
    }

    const TextDatabase& getTextDatabase() {
        static TextDatabase tb{};

        // If database is empty, load it up
        if (tb.empty()) {
            LoadTextData(tb);
        }

        return tb;
    }

    bool textObjectExists(const std::string& name) {
        return getTextDatabase().contains(name);
    }

    const Text& getTextObject(const std::string& name, Text::Type type /*= Text::STANDARD*/)
    {
        const auto& tb = getTextDatabase();
        if (!tb.contains(name)) {
            throw std::runtime_error("Text name \"" + name + "\" is not recognized.");
        }
        return tb.at(name).at(type);
    }

    const std::string& getTextStr(const std::string& name,
                        Text::Type type /*= Text::STANDARD*/,
                        Text::Language language /*= Text::ENGLISH*/)
    {
        const auto& tb = getTextDatabase();
        if (!tb.contains(name)) {
            throw std::runtime_error("Text name \"" + name + "\" is not recognized.");
        }

        if (!tb.at(name).at(type).mText.at(language).empty()) {
            return tb.at(name).at(type).mText.at(language);
        }

        // Return english if the other language's string is empty
        return tb.at(name).at(type).mText.at(language);
    }

    Text addColor(const Text& t, Text::Color color, int count /* = 1*/, bool forceAround /* = false*/) {
        const static std::unordered_map<Text::Color, std::string> colorStrings = {
            {Text::WHITE, "<white>"},
            {Text::RED, "<red>"},
            {Text::GREEN, "<green>"},
            {Text::LIGHT_BLUE, "<light blue>"},
            {Text::YELLOW, "<yellow>"},
            {Text::PURPLE, "<purple>"},
            {Text::ORANGE, "<orange>"},
            {Text::DARK_GREEN, "<dark green>"},
            {Text::BLUE, "<blue>"},
            {Text::SILVER, "<silver>"},
        };

        if (color == Text::Color::RAW) {
            return t;
        }

        if (!colorStrings.contains(color)) {
            throw std::runtime_error("Color enum value \"" + std::to_string(color) + "\" is not recognized.");
        }

        Text text = t;
        if (forceAround) {
            text = colorStrings.at(color) + text + colorStrings.at(Text::WHITE);
        }
        text.Replace("{", colorStrings.at(color), count);
        text.Replace("}", colorStrings.at(Text::WHITE), count);
        return text;
    }

    void applyMessageCodes(std::string& str) {
        using namespace std::string_literals;
        const static std::unordered_map<std::string, std::string> messageCodes = {
            {"<fast>",         "\x1A\x05\x00\x00\x01"s},
            {"<slow>",         "\x1A\x05\x00\x00\x02"s},
            {"<begin choice>", "\x1A\x05\x00\x00\x20"s},
            {"<male>",         "\x1A\x05\x06\x00\x02"s},
            {"<female>",       "\x1A\x05\x06\x00\x03"s},
            {"<choice 1>",     "\x1A\x06\x00\x00\x09\x01"s},
            {"<choice 2>",     "\x1A\x06\x00\x00\x09\x02"s},
            {"<choice 3>",     "\x1A\x06\x00\x00\x09\x03"s},
            {"<white>",        "\x1A\x06\xFF\x00\x00\x00"s},
            {"<red>",          "\x1A\x06\xFF\x00\x00\x01"s},
            {"<green>",        "\x1A\x06\xFF\x00\x00\x02"s},
            {"<light blue>",   "\x1A\x06\xFF\x00\x00\x03"s},
            {"<yellow>",       "\x1A\x06\xFF\x00\x00\x04"s},
            {"<purple>",       "\x1A\x06\xFF\x00\x00\x06"s},
            {"<orange>",       "\x1A\x06\xFF\x00\x00\x08"s},
            // custom colors
            {"<dark green>",   "\x1A\x06\xFF\x00\x00\x09"s},
            {"<blue>",         "\x1A\x06\xFF\x00\x00\x0A"s},
            {"<silver>",       "\x1A\x06\xFF\x00\x00\x0B"s},
        };

        for (const auto& [code, replacement] : messageCodes) {
            size_t pos = 0;
            while ((pos = str.find(code, pos)) != std::string::npos) {
                str.replace(pos, code.length(), replacement);
                pos += replacement.length();
            }
        }
    }
}; // namespace Text

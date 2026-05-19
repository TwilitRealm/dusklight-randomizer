#include "text.hpp"

#include <unordered_map>
#include <filesystem>
#include <fstream>

#include "yaml.hpp"

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


    Text::Type string_to_type(const std::string& str) {
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

    Text::Language string_to_language(const std::string& str) {
        std::unordered_map<std::string, Text::Language> strToLanguage = {
            {"english", Text::Language::ENGLISH},
        };

        if (strToLanguage.contains(str))
        {
            return strToLanguage.at(str);
        }

        throw std::runtime_error("Language \"" + str + "\" is not recognized.");
    }

    Text::Gender string_to_gender(const std::string& str)
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

    Text::Plurality string_to_plurality(const std::string& str)
    {
        if (str == "Plural") return Text::Plurality::PLURAL;
        return Text::Plurality::SINGULAR;
    }

    static void LoadTextData(TextDatabase& tb) {
        struct LanguageEntry {
            std::string language;
            std::string languageData;
        };
        auto files = std::to_array<LanguageEntry>({
            {"english", GET_EMBED_DATA(RANDO_DATA_PATH "text/languages/english.yaml")},
        });

        for (const auto& file : files) {
            auto language = string_to_language(file.language);
            auto textData = LOAD_EMBED_DATA(file.languageData);
            for (const auto& textNode : textData) {
                const auto& name = textNode.first.as<std::string>();
                for (const auto& typeNode : textNode.second) {
                    auto type = string_to_type(typeNode.first.as<std::string>());
                    auto typeData = typeNode.second;
                    const auto& text = typeData["Text"].as<std::string>();
                    tb[name][type].mtext[language] = text;
                    if (typeData["Gender"]) {
                        tb[name][type].mGender[language] = string_to_gender(typeData["Gender"].as<std::string>());
                    }
                    if (typeData["Plurality"]) {
                        tb[name][type].mPlurality[language] = string_to_plurality(typeData["Plurality"].as<std::string>());
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
        return tb.at(name).at(type).mtext.at(language);
    }

    void applyMessageCodes(std::string& str) {
        using namespace std::string_literals;
        const static std::unordered_map<std::string, std::string> messageCodes = {
            {"<fast>",       "\x1A\x05\x00\x00\x01"s },
            {"<slow>",       "\x1A\x05\x00\x00\x02"s },
            {"<choice 1>",   "\x1A\x06\x00\x00\x09\x01"s},
            {"<choice 2>",   "\x1A\x06\x00\x00\x09\x02"s},
            {"<choice 3>",   "\x1A\x06\x00\x00\x09\x03"s},
            {"<white>",      "\x1A\x06\xFF\x00\x00\x00"s},
            {"<red>",        "\x1A\x06\xFF\x00\x00\x01"s},
            {"<green>",      "\x1A\x06\xFF\x00\x00\x02"s},
            {"<light blue>", "\x1A\x06\xFF\x00\x00\x03"s},
            {"<yellow>",     "\x1A\x06\xFF\x00\x00\x04"s},
            {"<purple>",     "\x1A\x06\xFF\x00\x00\x06"s},
            {"<orange>",     "\x1A\x06\xFF\x00\x00\x08"s},
            // custom colors
            {"<dark green>", "\x1A\x06\xFF\x00\x00\x09"s},
            {"<blue>",       "\x1A\x06\xFF\x00\x00\x0A"s},
            {"<silver>",     "\x1A\x06\xFF\x00\x00\x0B"s},
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

#include "messages.hpp"

#include "JSystem/JMessage/control.h"
#include "d/d_msg_class.h"
#include "randomizer_context.hpp"
#include "custom_flow_ids.hpp"

#include "dusk/version.hpp"

#include <fmt/format.h>

// Format certain messages that need to have dynamic info in them
char* GetFormatedTextOverride(u32 key, std::string& text) {
    // Store formatted message in static buffer so it never goes away.
    // This is fine as long as we only ever need to format messages
    // for textboxes, but will cause issues if we need to use it for 
    // other UI elements
    static std::array<char, 256> buf;
    u32 value{};
    char* outIt;
    // For item counts, execItemGet hasn't run yet, so add one to the count
    switch (key) {
    case (0 << 16) | 325: // Group 0, id 325
        // Poe Soul get item text
        value = dComIfGs_getPohSpiritNum() + 1;
        outIt = fmt::vformat_to(buf.data(), text, fmt::make_format_args(value));
        break;
    case (0 << 16) | 335: // Group 0, id 335
        // Sky book characters get item text
        value = dComIfGs_getAncientDocumentNum() + 1;
        outIt = fmt::vformat_to(buf.data(), text, fmt::make_format_args(value));
        break;
    default:
        // No override, return original text
        return text.data();
    }

    // Null-terminate
    size_t len = std::distance(buf.data(), outIt);
    buf[len] = '\0';

    // Return overriden text
    return buf.data();
}

u8 getLanguageForOverride() {
    u8 language = randomizer::Text::ENGLISH;
    if (dusk::version::isRegionPal()) {
        language = dComIfGs_getPalLanguage();
    }/* else if (dusk::version::isRegionJpn()) {
        language = randomizer::Text::JAPANESE;
    }*/

    return language;
}

void HandleTextOverrides(JMessage::TControl* control, JMessage::TProcessor const* pProcessor, int groupID, int index) {
    if (randomizer_IsActive()) {
        // Get the entry for this message
        auto entry = static_cast<JMSMesgEntry_c*>(pProcessor->getMessageEntry_messageCode(groupID, index));
        if (!entry) {
            return;
        }

        // If the message id is >= 5000 then it's part of the stage file's message group
        // Otherwise it's part of group 0
        auto msgId = entry->message_id.host();
        u16 group = 0;
        if (msgId >= 5000) {
            group = dComIfGp_getStageStagInfo()->mMsgGroup;
        }

        u32 key = (group << 16) | msgId;
        auto& textOverrides = randomizer_GetContext().mTextOverrides;
        u8 language = getLanguageForOverride();
        if (textOverrides.at(language).contains(key)) {
            control->pMessageText_begin_ = GetFormatedTextOverride(key, textOverrides[language][key]);
        }
    }
}

bool HandleCustomText(JMessage::TControl* control, u16 msgId) {
    if (randomizer_IsActive()) {
        u32 key = (CUSTOM_BMG_GROUP << 16) | msgId;
        auto& textOverrides = randomizer_GetContext().mTextOverrides;
        u8 language = getLanguageForOverride();
        if (textOverrides.at(language).contains(key)) {
            control->pMessageText_begin_ = GetFormatedTextOverride(key, textOverrides[language][key]);
            defaultEntry.message_id = msgId;
            control->pEntry_ = &defaultEntry;
            return true;
        }
    }
    return false;
}

// Used in special cases
char* GetTextOverride(s16 groupID, u32 messageId) {
    u32 key = (groupID << 16) | messageId;
    auto& textOverrides = randomizer_GetContext().mTextOverrides;
    u8 language = getLanguageForOverride();
    if (textOverrides.at(language).contains(key)) {
        return GetFormatedTextOverride(key, textOverrides[language][key]);
    }
    return NULL;
}

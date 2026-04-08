#include "requirement.hpp"

#include "search.hpp"
#include "world.hpp"
#include "../utility/container.hpp"
#include "../utility/log.hpp"
#include "../utility/string.hpp"

#include <algorithm>
#include <ranges>
#include <iostream>

namespace tphdr::logic::requirement
{
    namespace FormTime
    {
        const std::vector<int> ALL_FORM_TIMES = {HUMAN_DAY, HUMAN_NIGHT, WOLF_DAY, WOLF_NIGHT};
        const std::vector<int> ALL_FORM_TIMES_AND_TWILIGHT = {HUMAN_DAY, HUMAN_NIGHT, WOLF_DAY, WOLF_NIGHT, TWILIGHT};
        const std::vector<int> ALL_FORM_AND_DAY_TIMES = {HUMAN_DAY, HUMAN_NIGHT, WOLF_DAY, WOLF_NIGHT, DAY, NIGHT};

        std::string to_string(const int& formTime)
        {
            std::string formTimeStr = "";
            if (formTime & HUMAN_DAY)
                formTimeStr += " Human_Day";
            if (formTime & HUMAN_NIGHT)
                formTimeStr += " Human_Night";
            if (formTime & WOLF_DAY)
                formTimeStr += " Wolf_Day";
            if (formTime & WOLF_NIGHT)
                formTimeStr += " Wolf_Night";
            if (formTime & TWILIGHT)
                formTimeStr += " Twilight";
            return formTimeStr;
        }
    } // namespace FormTime

    const extern Requirement NO_REQUIREMENT = Requirement{Type::NOTHING, {}};
    const extern Requirement IMPOSSIBLE_REQUIREMENT = Requirement{Type::IMPOSSIBLE, {}};

    std::string Requirement::to_string() const
    {
        std::string reqStr = "";
        tphdr::logic::item::Item* item;
        Requirement nestedReq;
        int count;
        int eventIndex;
        int macroIndex;
        switch (this->_type)
        {
            case Type::NOTHING:
                return "Nothing";

            case Type::IMPOSSIBLE:
                return "Impossible (Please discover an entrance first)";

            case Type::OR:
                for (const auto& arg : this->_args)
                {
                    nestedReq = std::get<Requirement>(arg);
                    if (nestedReq._type == Type::AND || nestedReq._type == Type::OR)
                    {
                        reqStr += "(";
                        reqStr += nestedReq.to_string();
                        reqStr += ")";
                    }
                    else
                    {
                        reqStr += nestedReq.to_string();
                    }
                    reqStr += " or ";
                }
                // pop off the last " or "
                for (auto i = 0; i < 4; i++)
                {
                    reqStr.pop_back();
                }
                return reqStr;

            case Type::AND:
                for (const auto& arg : this->_args)
                {
                    nestedReq = std::get<Requirement>(arg);
                    if (nestedReq._type == Type::AND || nestedReq._type == Type::OR)
                    {
                        reqStr += "(";
                        reqStr += nestedReq.to_string();
                        reqStr += ")";
                    }
                    else
                    {
                        reqStr += nestedReq.to_string();
                    }
                    reqStr += " and ";
                }
                // pop off the last " and "
                for (auto i = 0; i < 5; i++)
                {
                    reqStr.pop_back();
                }
                return reqStr;

            case Type::ITEM:
                item = std::get<tphdr::logic::item::Item*>(this->_args[0]);
                return item->GetName();

            case Type::COUNT:
                count = std::get<int>(this->_args[0]);
                item = std::get<tphdr::logic::item::Item*>(this->_args[1]);
                return "count(" + item->GetName() + ", " + std::to_string(count) + ")";

            case Type::EVENT:
                eventIndex = std::get<int>(this->_args[0]);
                return "'Event_" + std::to_string(eventIndex) + "'";

            case Type::MACRO:
                macroIndex = std::get<int>(this->_args[0]);
                return "'Macro_" + std::to_string(macroIndex) + "'";

            case Type::DAY:
                return "Day";

            case Type::NIGHT:
                return "Night";

            case Type::HUMAN_LINK:
                return "Human Link";

            case Type::WOLF_LINK:
                return "Wolf Link";

            case Type::TWILIGHT:
                return "Twilight";

            case Type::GOLDEN_BUGS:
                count = std::get<int>(this->_args[0]);
                return "golden_bugs(" + std::to_string(count) + ")";
            default:
                return reqStr;
        }
        return reqStr;
    }

    Requirement ParseRequirementString(const std::string& reqStr,
                                       tphdr::logic::world::World* world,
                                       const bool& forceLogic /* = false */)
    {
        Requirement req;
        std::string logicStr(reqStr);
        // First, we make sure that the expression has no missing or extra parenthesis
        // and that the nesting level at the beginning is the same at the end.
        //
        // Logic expressions are split up via spaces, but we only want to evaluate the parts of
        // the expression at the highest nesting level for the string that was passed in.
        // (We'll recursively call the function later to evaluate deeper levels.) So we replace
        // all the spaces on the highest nesting level with an arbitrarily chosen delimeter that shouldn't appear anywhere
        // in a logic statement (in req case: '+').
        int nestingLevel = 1;
        constexpr char delimeter = '+';
        for (auto& ch : logicStr)
        {
            if (ch == '(')
            {
                nestingLevel++;
            }
            else if (ch == ')')
            {
                nestingLevel--;
            }

            if (nestingLevel == 1 && ch == ' ')
            {
                ch = delimeter;
            }
        }

        // If the nesting level isn't the same as what we started with, then the logic
        // expression is invalid.
        if (nestingLevel != 1)
        {
            throw std::runtime_error("Extra or missing parenthesis within expression: \"" + reqStr + "\"");
        }

        // Next we split up the expression by the delimeter in the previous step
        size_t pos = 0;
        std::vector<std::string> splitLogicStr = {};
        while ((pos = logicStr.find(delimeter)) != std::string::npos)
        {
            // When parsing setting checks, take the entire expression
            // and the three components individually
            auto& chBefore = logicStr[pos - 1];
            auto& chAfter = logicStr[pos + 1];
            if (chBefore != '!' && chAfter != '!' && chBefore != '=' && chAfter != '=')
            {
                splitLogicStr.push_back(logicStr.substr(0, pos));
                logicStr.erase(0, pos + 1);
            }
            else
            {
                logicStr.erase(logicStr.begin() + pos);
            }
        }
        splitLogicStr.push_back(logicStr);

        // Once we have the different parts of our expression, we can use the number
        // of parts we have to determine what kind of expression it is.

        // If we only have one part...
        if (splitLogicStr.size() == 1)
        {
            std::string argStr = splitLogicStr[0];
            std::ranges::replace(argStr, '_', ' ');
            // First, see if we have nothing
            if (argStr == "Nothing")
            {
                req._type = tphdr::logic::requirement::Type::NOTHING;
                return req;
            }

            // Then Human Link...
            if (argStr == "Human Link")
            {
                req._type = tphdr::logic::requirement::Type::HUMAN_LINK;
                return req;
            }

            // Then Wolf Link...
            if (argStr == "Wolf Link")
            {
                req._type = tphdr::logic::requirement::Type::WOLF_LINK;
                return req;
            }

            // Then Twilight...
            if (argStr == "Twilight")
            {
                req._type = tphdr::logic::requirement::Type::TWILIGHT;
                return req;
            }

            // Then an event...
            if (argStr[0] == '\'')
            {
                req._type = tphdr::logic::requirement::Type::EVENT;
                std::string eventName(argStr.begin() + 1, argStr.end() - 1); // Remove quotes
                int eventId = world->GetEventIndex(eventName);

                req._args.emplace_back(eventId);
                return req;
            }

            // NOTE: Checking macros *MUST* come before checking items. Some macros use the exact same name as an item
            // and we want the macro to be used in req case instead of just the item

            // Then a macro...
            if (world->GetMacroIndex(argStr) != -1)
            {
                req._type = tphdr::logic::requirement::Type::MACRO;
                req._args.emplace_back(world->GetMacroIndex(argStr));
                return req;
            }

            // Then an item...
            if (world->GetItem(argStr, true) != nullptr)
            {
                auto item = world->GetItem(argStr);
                req._type = tphdr::logic::requirement::Type::ITEM;
                req._args.emplace_back(item);
                return req;
            }

            // Then a setting...
            else if (tphdr::utility::str::Contains(argStr, "!=", "=="))
            {
                bool equalComparison = tphdr::utility::str::Contains(argStr, "==");
                bool notEqualComparison = tphdr::utility::str::Contains(argStr, "!=");

                // Split up the comparison using the second comparison character (which will always be '=')
                auto compPos = argStr.rfind('=');
                std::string optionName(argStr.begin() + (compPos + 1), argStr.end());
                std::string settingName(argStr.begin(), argStr.begin() + (compPos - 1));

                // Check using the appropriate comparison function
                bool result = false;
                if (equalComparison)
                {
                    result = world->Setting(settingName) == optionName.c_str();
                }
                else if (notEqualComparison)
                {
                    result = world->Setting(settingName) != optionName.c_str();
                }

                if (result == true)
                {
                    req._type = tphdr::logic::requirement::Type::NOTHING;
                }
                else
                {
                    req._type = tphdr::logic::requirement::Type::IMPOSSIBLE;
                }
                return req;
            }
            // Then a count...
            else if (argStr.find("count") != std::string::npos)
            {
                req._type = tphdr::logic::requirement::Type::COUNT;
                // Since a count has two arguments (a number and an item), we have
                // to split up the string in the parenthesis into those arguments.

                // Get rid of parenthesis
                std::string countArgs(argStr.begin() + argStr.find('(') + 1, argStr.end() - 1);
                // Erase any spaces
                // countArgs.erase(std::remove(countArgs.begin(), countArgs.end(), ' '), countArgs.end());

                // Split up the arguments
                pos = 0;
                splitLogicStr = {};
                while ((pos = countArgs.find(", ")) != std::string::npos)
                {
                    splitLogicStr.push_back(countArgs.substr(0, pos));
                    countArgs.erase(0, pos + 2);
                }
                splitLogicStr.push_back(countArgs);

                // Get the arguments
                auto& itemName = splitLogicStr[0];
                int count = std::stoi(splitLogicStr[1]);
                auto item = world->GetItem(itemName);
                req._args.emplace_back(count);
                req._args.emplace_back(item);
                return req;
            }

            // Then Day...
            if (argStr == "Day")
            {
                req._type = tphdr::logic::requirement::Type::DAY;
                return req;
            }

            // Then Night...
            if (argStr == "Night")
            {
                req._type = tphdr::logic::requirement::Type::NIGHT;
                return req;
            }

            // And finally a health check
            // else if (argStr.find("health") != std::string::npos)
            // {
            //     req._type = tphdr::logic::requirement::Type::HEALTH;
            //     std::string numHeartsStr(argStr.begin() + argStr.find('(') + 1, argStr.end() - 1);
            //     int numHearts = std::stoi(numHeartsStr);
            //     req._args.emplace_back(numHearts);
            //     return req;
            // }

            // Check Impossible down here since it's very unlikely
            else if (argStr == "Impossible")
            {
                req._type = tphdr::logic::requirement::Type::IMPOSSIBLE;
                return req;
            }

            // Check golden bugs last since it's least likely
            else if (argStr.find("golden bugs") != std::string::npos)
            {
                req._type = tphdr::logic::requirement::Type::GOLDEN_BUGS;
                // Get rid of parenthesis
                std::string countArg(argStr.begin() + argStr.find('(') + 1, argStr.end() - 1);
                int count = std::stoi(countArg);
                req._args.emplace_back(count);
                return req;
            }

            throw std::runtime_error("Unrecognized logic symbol: \"" + reqStr + "\"");
        }

        // If our expression has two parts, then we don't know what that is
        if (splitLogicStr.size() == 2)
        {
            throw std::runtime_error("Unrecognized 2 part expression: " + reqStr);
        }

        // If we have more than two parts to our expression, then we have either "and"
        // or "or".
        bool andType = tphdr::utility::container::ElementInContainer(splitLogicStr, "and");
        bool orType = tphdr::utility::container::ElementInContainer(splitLogicStr, "or");

        // If we have both of them, there's a problem with the logic expression
        if (andType && orType)
        {
            throw std::runtime_error("\"and\" & \"or\" in same nesting level when parsing \"" + reqStr + "\"");
        }

        if (andType || orType)
        {
            // Set the appropriate type
            if (andType)
            {
                req._type = tphdr::logic::requirement::Type::AND;
            }
            else
            {
                req._type = tphdr::logic::requirement::Type::OR;
            }

            // Once we know the type, we can erase the "and"s or "or"s and are left with just the deeper
            // expressions to be logically operated on.
            tphdr::utility::container::FilterAndEraseFromVector(splitLogicStr,
                                                                [](const std::string& arg)
                                                                { return arg == "and" || arg == "or"; });

            // For each deeper expression, parse it and add it as an argument to the
            // Requirement
            for (auto& reqStr : splitLogicStr)
            {
                // Get rid of parenthesis surrounding each deeper expression
                if (reqStr[0] == '(')
                {
                    reqStr = reqStr.substr(1, reqStr.length() - 2);
                }
                req._args.push_back(ParseRequirementString(reqStr, world, forceLogic));
            }
        }

        if (req._type != tphdr::logic::requirement::Type::INVALID)
        {
            return req;
        }
        // If we've reached req point, we weren't able to determine a logical operator within the expression
        throw std::runtime_error("Could not determine logical operator type from expression: \"" + reqStr + "\"");
        return req;
    }

    bool EvaluateRequirementAtFormTime(const tphdr::logic::requirement::Requirement& req,
                                       tphdr::logic::search::Search* search,
                                       const int& formTime,
                                       tphdr::logic::world::World* world)
    {
        tphdr::logic::item::Item* item;
        int count;
        int eventIndex;
        int macroIndex;
        switch (req._type)
        {
            case Type::NOTHING:
                return true;

            case Type::IMPOSSIBLE:
                return false;

            case Type::OR:
                return std::any_of(
                    req._args.begin(),
                    req._args.end(),
                    [&](const auto& arg)
                    { return EvaluateRequirementAtFormTime(std::get<Requirement>(arg), search, formTime, world); });

            case Type::AND:
                return std::all_of(
                    req._args.begin(),
                    req._args.end(),
                    [&](const auto& arg)
                    { return EvaluateRequirementAtFormTime(std::get<Requirement>(arg), search, formTime, world); });

            case Type::ITEM:
                item = std::get<tphdr::logic::item::Item*>(req._args[0]);
                return search->_ownedItems.contains(item);

            case Type::COUNT:
                count = std::get<int>(req._args[0]);
                item = std::get<tphdr::logic::item::Item*>(req._args[1]);
                return search->_ownedItems.count(item) >= count;

            case Type::EVENT:
                eventIndex = std::get<int>(req._args[0]);
                return search->_ownedEvents.contains(eventIndex);

            case Type::MACRO:
                macroIndex = std::get<int>(req._args[0]);
                return EvaluateRequirementAtFormTime(world->GetMacro(macroIndex), search, formTime, world);

            case Type::DAY:
                return formTime & FormTime::DAY;

            case Type::NIGHT:
                return formTime & FormTime::NIGHT;

            case Type::HUMAN_LINK:
                return formTime & FormTime::HUMAN;

            case Type::WOLF_LINK:
                return formTime & FormTime::WOLF;

            case Type::TWILIGHT:
                return formTime & FormTime::TWILIGHT;

            case Type::GOLDEN_BUGS:
                count = std::get<int>(req._args[0]);
                return std::count_if(search->_ownedItems.begin(),
                                     search->_ownedItems.end(),
                                     [](const auto& item) { return item->IsGoldenBug(); }) >= count;
            default:
                return false;
        }
        return false;
    }

    EvalSuccess EvaluateEventRequirement(tphdr::logic::search::Search* search, tphdr::logic::area::EventAccess* event)
    {
        auto& formTime = search->_areaFormTime[event->GetArea()];
        if (EvaluateRequirementAtFormTime(event->GetRequirement(), search, formTime, event->GetArea()->GetWorld()))
        {
            return EvalSuccess::COMPLETE;
        }
        return EvalSuccess::NONE;
    }

    EvalSuccess EvaluateExitRequirement(tphdr::logic::search::Search* search, tphdr::logic::entrance::Entrance* exit)
    {
        // Some exits in the middle of entrance shuffling will not have a connected area. Ignore these
        if (exit->GetConnectedArea() == nullptr)
        {
            return EvalSuccess::UNNECESSARY;
        }

        // If the exit is currently disabled, don't try it
        if (exit->IsDisabled())
        {
            return EvalSuccess::NONE;
        }

        auto& exitFormTimeCache = exit->GetWorld()->GetExitTimeFormCache();
        auto parentArea = exit->GetParentArea();
        auto connectedArea = exit->GetConnectedArea();
        auto parentAreaFormTime = search->_areaFormTime[parentArea];
        auto& connectedAreaFormTime = search->_areaFormTime[connectedArea];
        auto potentialExitFormTimes = (exitFormTimeCache.contains(exit) ? exitFormTimeCache[exit] : FormTime::ALL);

        // LOG_TO_DEBUG("Trying " + connectedArea->GetName());

        auto connectedAreaTwilightCleared = connectedArea->TwilightCleared(search);
        if (!connectedAreaTwilightCleared)
        {
            // LOG_TO_DEBUG("Added Twilight");
            parentAreaFormTime |= FormTime::TWILIGHT;
            potentialExitFormTimes |= FormTime::TWILIGHT;
        }

        // Calculate the potential form times that we could spread to the connected area. These are the form times
        // which the connected area does not have that the parent area has, and that the exit can potentially pass on
        // to the connected area
        auto potentialFormTimeSpread = ~connectedAreaFormTime & (parentAreaFormTime & potentialExitFormTimes);

        // LOG_TO_DEBUG("Potential spreads: " + FormTime::to_string(potentialFormTimeSpread));

        // If there's no potential to spread FormTime, then return early
        if (potentialFormTimeSpread == FormTime::NONE)
        {
            // LOG_TO_DEBUG("No potential formtime spread");
            return EvalSuccess::NONE;
        }

        // Check each form time individually and spread the ones which succeed. If any of them pass, set the evaluation success
        // to partial.
        auto evalSuccess = EvalSuccess::NONE;
        const auto& formTimes = connectedAreaTwilightCleared ? FormTime::ALL_FORM_TIMES : FormTime::ALL_FORM_TIMES_AND_TWILIGHT;
        for (const auto& formTime : formTimes)
        {
            if (formTime & potentialFormTimeSpread)
            {
                if (EvaluateRequirementAtFormTime(exit->GetRequirement(), search, formTime, exit->GetWorld()))
                {
                    if (!connectedAreaTwilightCleared)
                    {
                        if (~connectedAreaFormTime & FormTime::TWILIGHT)
                        {
                            // LOG_TO_DEBUG("Spread Twilight to " + connectedArea->GetName());
                            connectedAreaFormTime |= FormTime::TWILIGHT;
                            evalSuccess = EvalSuccess::PARTIAL;
                        }
                    }
                    else if (formTime != FormTime::TWILIGHT)
                    {
                        // LOG_TO_DEBUG("Spread" + FormTime::to_string(formTime) + " to " + connectedArea->GetName());
                        connectedAreaFormTime |= formTime;
                        evalSuccess = EvalSuccess::PARTIAL;
                    }
                }
            }
            else
            {
                // LOG_TO_DEBUG(FormTime::to_string(formTime) + " is not a potential timespread.");
            }
        }

        if (evalSuccess != EvalSuccess::NONE)
        {
            search->ExpandFormTimes(connectedArea);
            // If the connected area now has complete access, then we mark a complete success instead of just a partial one
        }

        if (connectedAreaTwilightCleared && ((connectedAreaFormTime & potentialExitFormTimes) == potentialExitFormTimes))
        {
            evalSuccess = EvalSuccess::COMPLETE;
        }

        return evalSuccess;
    }

    EvalSuccess EvaluateLocationRequirement(tphdr::logic::search::Search* search, tphdr::logic::area::LocationAccess* locAccess)
    {
        auto& formTime = search->_areaFormTime[locAccess->GetArea()];
        if (EvaluateRequirementAtFormTime(locAccess->GetRequirement(), search, formTime, locAccess->GetArea()->GetWorld()))
        {
            return EvalSuccess::COMPLETE;
        }
        return EvalSuccess::NONE;
    }
} // namespace tphdr::logic::requirement

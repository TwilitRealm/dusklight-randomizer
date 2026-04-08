#pragma once

#include "item.hpp"
#include "requirement.hpp"

#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace randomizer::logic::world
{
    class World;
}

namespace randomizer::logic::area
{
    class LocationAccess;
}

namespace randomizer::logic::location
{
    class Location
    {
       public:
        Location(const int& id,
                 const std::string& name,
                 std::unordered_set<std::string> categories,
                 randomizer::logic::world::World* world,
                 randomizer::logic::item::Item* originalItem,
                 const bool& goalLocation,
                 const std::string& hintPriority);

        int GetID() const;
        std::string GetName() const;
        randomizer::logic::world::World* GetWorld() const;
        bool IsGoalLocation() const;
        void SetCurrentItem(randomizer::logic::item::Item* currentItem);
        randomizer::logic::item::Item* GetCurrentItem() const;
        void RemoveCurrentItem();
        bool IsEmpty() const;
        randomizer::logic::item::Item* GetOriginalItem() const;
        randomizer::logic::item::Item* GetTrackedItem() const;
        void SetKnownVanillaItem(const bool& hasKnownVanillaItem);
        bool HasKnownVanillaItem() const;
        void SetProgression(const bool& progression);
        bool IsProgression() const;
        void SetHinted(const bool& hinted);
        bool IsHinted() const;
        void AddLocationAccess(randomizer::logic::area::LocationAccess* locAcc);
        std::list<randomizer::logic::area::LocationAccess*> GetAccessList() const;
        void AddForbiddenItem(randomizer::logic::item::Item* forbiddenItem);
        const std::unordered_set<randomizer::logic::item::Item*>& GetForbiddenItems();
        void SetComputedRequirement(const randomizer::logic::requirement::Requirement& computedRequirement);
        randomizer::logic::requirement::Requirement GetComputedRequirement();
        void SetRegisteredLocationCategories(std::unordered_set<std::string>* registeredLocationCategories);

        /**
         *  @brief Checks to see if the location has all the passed in categories. If a passed in category was never registred,
         *  a std::runtime_error will be thrown.
         *  @param categoryNames paramater pack of string representations of category names
         *  @returns true if all passed in categories are present, false otherwise
         */
        template<class... Types>
        bool HasCategories(Types... categoryNames) const
        {
            for (const auto& categoryName : {categoryNames...})
            {
                if (this->_registeredLocationCategories != nullptr &&
                    !this->_registeredLocationCategories->contains(categoryName))
                {
                    throw std::runtime_error(std::string("Category \"") + categoryName + "\" is not used by any locations");
                }
                if (!this->_categories.contains(categoryName))
                {
                    return false;
                }
            }

            return true;
        }

       private:
        int _id = -1;
        std::string _name = "";
        std::unordered_set<std::string> _categories = {};
        randomizer::logic::world::World* _world;
        randomizer::logic::item::Item* _originalItem = randomizer::logic::item::Nothing.get();
        bool _goalLocation = false;
        randomizer::logic::item::Item* _currentItem = randomizer::logic::item::Nothing.get();
        bool _hasKnownVanillaItem = false;
        std::list<randomizer::logic::area::LocationAccess*> _locationAccessList = {};
        bool _progression = true; // Set as false later if applicable
        bool _hinted = false;
        std::string _hintPriority = "Never";
        std::unordered_set<randomizer::logic::item::Item*> _forbiddenItems = {};
        randomizer::logic::requirement::Requirement _computedRequirement;
        /**
         *  @brief _registeredLocationCategories is the set of all categories that are processed after reading locations.yaml.
         * This structure is held in the World class and every location in that world has a pointer to it.
         * We can't call it from the world directly since the function we want to use it in is templated in this class.
         */
        std::unordered_set<std::string>* _registeredLocationCategories = nullptr;

        // Potential tracker stuff
        randomizer::logic::item::Item* _trackedItem = randomizer::logic::item::Nothing.get();
    };

    using LocationPool = std::vector<Location*>;
} // namespace randomizer::logic::location

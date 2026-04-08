#pragma once

#include "item.hpp"
#include "requirement.hpp"

#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace tphdr::logic::world
{
    class World;
}

namespace tphdr::logic::area
{
    class LocationAccess;
}

namespace tphdr::logic::location
{
    class Location
    {
       public:
        Location(const int& id,
                 const std::string& name,
                 std::unordered_set<std::string> categories,
                 tphdr::logic::world::World* world,
                 tphdr::logic::item::Item* originalItem,
                 const bool& goalLocation,
                 const std::string& hintPriority);

        int GetID() const;
        std::string GetName() const;
        tphdr::logic::world::World* GetWorld() const;
        bool IsGoalLocation() const;
        void SetCurrentItem(tphdr::logic::item::Item* currentItem);
        tphdr::logic::item::Item* GetCurrentItem() const;
        void RemoveCurrentItem();
        bool IsEmpty() const;
        tphdr::logic::item::Item* GetOriginalItem() const;
        tphdr::logic::item::Item* GetTrackedItem() const;
        void SetKnownVanillaItem(const bool& hasKnownVanillaItem);
        bool HasKnownVanillaItem() const;
        void SetProgression(const bool& progression);
        bool IsProgression() const;
        void SetHinted(const bool& hinted);
        bool IsHinted() const;
        void AddLocationAccess(tphdr::logic::area::LocationAccess* locAcc);
        std::list<tphdr::logic::area::LocationAccess*> GetAccessList() const;
        void AddForbiddenItem(tphdr::logic::item::Item* forbiddenItem);
        const std::unordered_set<tphdr::logic::item::Item*>& GetForbiddenItems();
        void SetComputedRequirement(const tphdr::logic::requirement::Requirement& computedRequirement);
        tphdr::logic::requirement::Requirement GetComputedRequirement();
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
        tphdr::logic::world::World* _world;
        tphdr::logic::item::Item* _originalItem = tphdr::logic::item::Nothing.get();
        bool _goalLocation = false;
        tphdr::logic::item::Item* _currentItem = tphdr::logic::item::Nothing.get();
        bool _hasKnownVanillaItem = false;
        std::list<tphdr::logic::area::LocationAccess*> _locationAccessList = {};
        bool _progression = true; // Set as false later if applicable
        bool _hinted = false;
        std::string _hintPriority = "Never";
        std::unordered_set<tphdr::logic::item::Item*> _forbiddenItems = {};
        tphdr::logic::requirement::Requirement _computedRequirement;
        /**
         *  @brief _registeredLocationCategories is the set of all categories that are processed after reading locations.yaml.
         * This structure is held in the World class and every location in that world has a pointer to it.
         * We can't call it from the world directly since the function we want to use it in is templated in this class.
         */
        std::unordered_set<std::string>* _registeredLocationCategories = nullptr;

        // Potential tracker stuff
        tphdr::logic::item::Item* _trackedItem = tphdr::logic::item::Nothing.get();
    };

    using LocationPool = std::vector<Location*>;
} // namespace tphdr::logic::location

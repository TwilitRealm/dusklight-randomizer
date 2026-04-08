#include "location.hpp"

#include "world.hpp"
#include "../utility/log.hpp"

namespace tphdr::logic::location
{
    Location::Location(const int& id,
                       const std::string& name,
                       std::unordered_set<std::string> categories,
                       tphdr::logic::world::World* world,
                       tphdr::logic::item::Item* originalItem,
                       const bool& goalLocation,
                       const std::string& hintPriority):
        _id(id),
        _name(name),
        _categories(categories),
        _world(world),
        _originalItem(originalItem),
        _goalLocation(goalLocation),
        _hintPriority(hintPriority)
    {
        this->_computedRequirement._type = tphdr::logic::requirement::Type::IMPOSSIBLE;
    }

    int Location::GetID() const
    {
        return this->_id;
    }

    std::string Location::GetName() const
    {
        return this->_name;
    }

    tphdr::logic::world::World* Location::GetWorld() const
    {
        return this->_world;
    }

    bool Location::IsGoalLocation() const
    {
        return this->_goalLocation;
    }

    void Location::SetCurrentItem(tphdr::logic::item::Item* item)
    {
        LOG_TO_DEBUG("Placed " + item->GetName() + " at " + this->GetName());
        this->_currentItem = item;
    }

    tphdr::logic::item::Item* Location::GetCurrentItem() const
    {
        return this->_currentItem;
    }

    void Location::RemoveCurrentItem()
    {
        LOG_TO_DEBUG("Removed " + this->GetCurrentItem()->GetName() + " at " + this->GetName());
        this->_currentItem = tphdr::logic::item::Nothing.get();
    }

    bool Location::IsEmpty() const
    {
        return this->_currentItem == tphdr::logic::item::Nothing.get();
    }

    tphdr::logic::item::Item* Location::GetOriginalItem() const
    {
        return this->_originalItem;
    }

    tphdr::logic::item::Item* Location::GetTrackedItem() const
    {
        return this->_trackedItem;
    }

    void Location::SetKnownVanillaItem(const bool& hasKnownVanillaItem)
    {
        this->_hasKnownVanillaItem = hasKnownVanillaItem;
    }

    bool Location::HasKnownVanillaItem() const
    {
        return this->_hasKnownVanillaItem;
    }

    void Location::SetProgression(const bool& progression)
    {
        this->_progression = progression;
        LOG_TO_DEBUG(this->GetName() + " progression status set to " + (progression ? " true" : "false"));
    }

    bool Location::IsProgression() const
    {
        return this->_progression;
    }

    void Location::SetHinted(const bool& hinted)
    {
        this->_hinted = hinted;
    }

    bool Location::IsHinted() const
    {
        return this->_hinted;
    }

    void Location::AddLocationAccess(tphdr::logic::area::LocationAccess* locAcc)
    {
        this->_locationAccessList.push_back(locAcc);
    }

    std::list<tphdr::logic::area::LocationAccess*> Location::GetAccessList() const
    {
        return this->_locationAccessList;
    }

    void Location::AddForbiddenItem(tphdr::logic::item::Item* forbiddenItem)
    {
        this->_forbiddenItems.insert(forbiddenItem);
        LOG_TO_DEBUG(forbiddenItem->GetName() + " is forbidden from being placed at " + this->GetName());
    }

    const std::unordered_set<tphdr::logic::item::Item*>& Location::GetForbiddenItems()
    {
        return this->_forbiddenItems;
    }

    void Location::SetComputedRequirement(const tphdr::logic::requirement::Requirement& computedRequirement)
    {
        this->_computedRequirement = computedRequirement;
    }

    tphdr::logic::requirement::Requirement Location::GetComputedRequirement()
    {
        return this->_computedRequirement;
    }

    void Location::SetRegisteredLocationCategories(std::unordered_set<std::string>* registeredLocationCategories)
    {
        this->_registeredLocationCategories = registeredLocationCategories;
    }
} // namespace tphdr::logic::location

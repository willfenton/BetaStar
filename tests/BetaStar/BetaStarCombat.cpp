#include "BetaStar.h"

using namespace sc2;

void BetaStar::GatherIntelligence(const Unit *unit)
{
    // detect air units
    if (!has_flying && unit->is_flying)
    {
        has_flying = true;
        std::cout << "Enemy has flying units" << std::endl;
    }

    // detect cloaked units
    if (!has_cloaked && (unit->cloak == Unit::Cloaked || unit->cloak == Unit::CloakedDetected))
    {
        has_cloaked = true;
        std::cout << "Enemy has cloaked units" << std::endl;
    }

    // detect detector units (if they don't have any, we can lean into cloaked units)
    if (!has_detection && unit->detect_range > 0.1f)
    {
        has_detection = true;
        std::cout << "Enemy has detection units" << std::endl;
    }

    // detect a rush and/or other strats that pose threat to our main base
    // using 50.0f since that covers the main base and can see into the closest expansion
    if (Distance2D(m_starting_pos, unit->pos) <= 50.0f)
    {
        // check for buildings near our base
        if (!building_near_our_base && (IsStructure(unit->unit_type) || unit->unit_type == UNIT_TYPEID::TERRAN_SIEGETANKSIEGED))
        {
            building_near_our_base = true;
        }

        // Rush detection

        //valid for roughly five minutes. After that, skip it
        uint32_t maxRushTime = 10000;

        //Each unit in a "rush" needs to be detected with less than this time elapsing between each sighting
        // 320 steps should be roughly 10 seconds
        uint32_t rushWindow = 320;

        // consider 5 early units as a rush (though we may want to increase this number)
        size_t rushNumber = 5;

        uint32_t currentGameLoop = Observation()->GetGameLoop();

        if (!rush_detected && currentGameLoop <= maxRushTime)
        {
            if (currentGameLoop >= last_detected_at_base_loop + 320)
            {
                rush_units.clear();
            }

            // using a set, so same unit can't be counted multiple times
            rush_units.insert(unit);

            if (rush_units.size() >= rushNumber)
            {
                rush_detected = true;
                std::cout << "Rush Detected" << std::endl;
            }

            last_detected_at_base_loop = currentGameLoop;
        }
    }
}

void BetaStar::TrainBalancedArmy()
{
    // get our own modified army count instead of using the built-in function since we may not want to take
    // into account units that aren't managed by this function
    size_t modifiedArmyCount = 0;
    for (UNIT_TYPEID tID : managed_unit_types)
    {
        modifiedArmyCount += CountUnitType(tID);
    }

    for (UNIT_TYPEID tID : managed_unit_types)
    {
        // always be building toward a future army so that armies don't get stuck at perfect ratios with a small number of units
        if (CountUnitType(tID) < ceil(army_ratios[tID] * (modifiedArmyCount + 1)))
        {
            // we're training a unit, so the army count goes up
            if (TrainUnit(tID))
            {
                ++modifiedArmyCount;
            }
        }
    }
}

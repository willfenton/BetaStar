#include "BetaStar.h"

using namespace sc2;

void BetaStar::GatherIntelligence(const Unit *unit)
{
    // detect enemy race
    if (enemy_race == Race::Random)
    {
        enemy_race = all_unit_type_data[unit->unit_type].race;
        switch (enemy_race)
        {
            case Race::Protoss:
                std::cout << "Enemy is Protoss" << std::endl;
                break;
            case Race::Terran:
                std::cout << "Enemy is Terran" << std::endl;
                break;
            case Race::Zerg:
                std::cout << "Enemy is Zerg" << std::endl;
                break;
            default:
                std::cout << "Enemy is [ERROR]" << std::endl;
                break;
        }
    }

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
            std::cout << "Enemy building near our base" << std::endl;
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

void BetaStar::BaseDefenseMacro(const Units units)
{
    Units enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy);

    // if there's an enemy close, attack it
    for (const auto& unit : enemy_units) {
        if (unit->display_type == Unit::DisplayType::Visible) {
            float distance = DistanceSquared2D(m_starting_pos, unit->pos);
            if (distance < 2500) {
                Actions()->UnitCommand(units, ABILITY_ID::ATTACK, unit->pos);
                break;
            }
        }
    }

    // TODO: Better clustering/positioning within base when idle
    for (const auto& unit : units) {
        float distance_from_base = DistanceSquared2D(unit->pos, m_starting_pos);
        if (!m_attacking && distance_from_base > 2500) {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, m_starting_pos);
        }
        else if (unit->orders.size() == 0 && distance_from_base >= 500 && distance_from_base <= 2500) {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, m_starting_pos);
        }
    }
}

void BetaStar::EnemyBaseAttackMacro(const Units units)
{
    Units enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy);
    Point2D unitCentroid = GetUnitsCentroid(units);

    if (enemy_units.size() == 0) {
        // if we're on top of the old base and can't find units, search for another base
        if (!m_searching_new_enemy_base && DistanceSquared2D(unitCentroid, m_enemy_base_pos) < 100)
        {
            m_searching_new_enemy_base = true;
        }

        if (m_searching_new_enemy_base)
        {
            // we've reached a new location and there isn't anything here - go to the next one
            if (DistanceSquared2D(unitCentroid, m_expansion_locations[m_current_search_index]) < 100)
            {
                m_current_search_index = (m_current_search_index + 1) % m_expansion_locations.size();
            }
            // continue the march to the new location to check
            else
            {
                Actions()->UnitCommand(units, ABILITY_ID::ATTACK, m_expansion_locations[m_current_search_index]);
            }
        }
        // still think we can march to enemy base to wipe them out - continue going there
        else
        {
            Actions()->UnitCommand(units, ABILITY_ID::ATTACK, m_enemy_base_pos);
        }
    }
    else {
        // TODO: Replace with targeting micro
        Actions()->UnitCommand(units, ABILITY_ID::ATTACK, GetClosestUnit(unitCentroid, enemy_units)->pos);
    }
}

void BetaStar::StalkerBlinkMicro()
{
    Units stalkers = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_STALKER);

    for (const auto& stalker : stalkers) {
        if (stalker->shield == 0 && stalker->health < stalker->health_max) {
            if (m_blink_researched) {
                const Unit* target_unit = Observation()->GetUnit(stalker->engaged_target_tag);
                Point2D blink_location = m_starting_pos;
                if (stalker->shield < 1) {
                    if (!stalker->orders.empty()) {
                        if (Observation()->GetUnit(stalker->engaged_target_tag) != nullptr) {
                            Vector2D diff = stalker->pos - target_unit->pos;
                            Normalize2D(diff);
                            blink_location = stalker->pos + diff * 7.0f;
                        }
                        else {
                            Vector2D diff = stalker->pos - m_starting_pos;
                            Normalize2D(diff);
                            blink_location = stalker->pos - diff * 7.0f;
                        }
                        Actions()->UnitCommand(stalker, ABILITY_ID::EFFECT_BLINK, blink_location);
                    }
                }
            }
        }
    }
}

#include "BetaStar.h"

using namespace sc2;

void BetaStar::GatherIntelligence(const Unit *unit)
{
    // detect enemy race
    if (enemy_race == Race::Random)
    {
        enemy_race = all_unit_type_data[unit->unit_type].race;
        /*switch (enemy_race)
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
        }*/
    }

    // detect air units
    if (!has_flying && unit->is_flying)
    {
        if (!all_unit_type_data[unit->unit_type].weapons.empty())
        {
            has_flying = true;
            //std::cout << "Enemy has flying units that can attack" << std::endl;
        }
    }

    // detect cloaked units
    if (!has_cloaked && (unit->cloak == Unit::Cloaked || unit->cloak == Unit::CloakedDetected))
    {
        if (!all_unit_type_data[unit->unit_type].weapons.empty())
        {
            has_cloaked = true;
            //std::cout << "Enemy has cloaked units that can attack" << std::endl;
        }
    }

    // detect detector units (if they don't have any, we can lean into cloaked units)
    if (!has_detection && unit->detect_range > 0.1f)
    {
        has_detection = true;
        //std::cout << "Enemy has detection units" << std::endl;
    }

    // detect a rush and/or other strats that pose threat to our main base
    // using 50.0f since that covers the main base and can see into the closest expansion
    if (Distance2D(m_starting_pos, unit->pos) <= 50.0f)
    {
        // check for buildings near our base
        if (!building_near_our_base && (IsStructure(unit->unit_type) || unit->unit_type == UNIT_TYPEID::TERRAN_SIEGETANKSIEGED))
        {
            building_near_our_base = true;
            //std::cout << "Enemy building near our base" << std::endl;
        }

        // Rush detection

        //valid for maxRushTime seconds, after that, it's not a rush
        float maxRushTime = 300.0f; // 300 seconds = 5 minutes
        //Each unit in a "rush" needs to be detected with less than this time elapsing between each sighting
        float rushWindow = 10.0f;
        // consider 5 early units as a rush (though we may want to increase this number)
        size_t rushNumber = 5;

        float currentTime = GetGameTime();

        if (!rush_detected && currentTime <= maxRushTime)
        {
            if (currentTime >= last_detected_at_base_time + rushWindow)
            {
                rush_units.clear();
            }

            // using a set, so same unit can't be counted multiple times
            rush_units.insert(unit);

            if (rush_units.size() >= rushNumber)
            {
                rush_detected = true;
                //std::cout << "Rush Detected" << std::endl;
            }

            last_detected_at_base_time = currentTime;
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
    Units enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy, IsNotCloaked(IsVisible(IsNear(m_starting_pos, 50.0f))));

    // if there are valid enemies close, attack them
    TargetingMicro(units, enemy_units);

    // TODO: Better clustering/positioning within base when idle
    for (const auto& unit : units) {
        float distance_from_base = DistanceSquared2D(unit->pos, m_starting_pos);
        if (!m_attacking && distance_from_base > 2500) {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, m_army_rally_pos);
        }
        else if (unit->orders.size() == 0 && distance_from_base >= 500 && distance_from_base <= 2500) {
            Actions()->UnitCommand(unit, ABILITY_ID::MOVE, m_army_rally_pos);
        }
    }
}

//Note that our current targeting strategy is to attack the highest priority unit. If there is a tie we attack the closest
//highest priority unit. We also ensure that units that cannot attack flying units, get to attack the highest priority and
//closest ground enemy unit if such an enemy unit exists
void BetaStar::EnemyBaseAttackMacro(const Units units)
{
    Units enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy, IsNotCloaked(IsVisible()));

    if (enemy_units.size() == 0) {

        // We may have hidden units to attack
        enemy_units = Observation()->GetUnits(Unit::Alliance::Enemy, IsNotCloaked());
        if (enemy_units.size() == 0)
        {
            if (!m_searching_new_enemy_base)
            {
                for (const Unit *unit : units)
                {
                    // if we're on top of the old base and can't find units, trigger search and destroy
                    if (DistanceSquared2D(unit->pos, m_enemy_base_pos) < 100)
                    {
                        m_searching_new_enemy_base = true;
                        break;
                    }
                }
            }

            if (m_searching_new_enemy_base)
            {
                // re-fill the list of locations to search if empty
                if (m_open_expansion_locations.empty())
                {
                    for (Point2D point : m_expansion_locations)
                    {
                        m_open_expansion_locations.push_back(point);
                    }
                }

                for (const Unit *unit : units)
                {
                    // Things to do if a unit is close to a location to search
                    for (auto iter = m_open_expansion_locations.begin(); iter != m_open_expansion_locations.end(); )
                    {
                        // Is a unit close to an expansion location without seeing an enemy (or is the expansion location invalid?)
                        if (!AlmostEqual(*iter, Point2D(0.0f, 0.0f)) && DistanceSquared2D(unit->pos, *iter) < 100)
                        {
                            // Remove searched base location from list, but store it temporarily for some checks
                            Point2D storedPoint = *iter;
                            iter = m_open_expansion_locations.erase(iter);

                            // Expansion location has been searched. All units that were going there should go to a different location instead.
                            for (const Unit *subUnit : units)
                            {
                                for (UnitOrder order : subUnit->orders)
                                {
                                    if (AlmostEqual(order.target_pos, storedPoint))
                                    {
                                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, GetRandomEntry(m_open_expansion_locations));
                                    }
                                }
                            }
                        }
                        else
                        {
                            ++iter;
                        }
                    }

                    // March to a new location unless otherwise engaged
                    if (unit->orders.empty())
                    {
                        Actions()->UnitCommand(unit, ABILITY_ID::ATTACK, GetRandomEntry(m_open_expansion_locations));
                    }
                }
            }
            // still think we can march to enemy main base to wipe them out - continue going there
            else
            {
                Actions()->UnitCommand(units, ABILITY_ID::ATTACK, m_enemy_base_pos);
            }
        }
        // March toward nearest hidden enemy
        else
        {
            Actions()->UnitCommand(units, ABILITY_ID::ATTACK, GetClosestUnit(GetUnitsCentroid(units), enemy_units)->pos);
        }
    }
    else //Note: Now we know that enemy_units is non-empty in this block of code
    { 
        TargetingMicro(units, enemy_units);
    }
}

void BetaStar::TargetingMicro(const Units units, Units enemy_units)
{
    // Must have valid targets and valid units that need targets
    if (enemy_units.empty() || units.empty())
    {
        return;
    }

    Point2D army_centroid = GetUnitsCentroidNearPoint(units, 0.25f, GetUnitsCentroid(enemy_units));;

    //Sort Enemy Units By Targeting Priority
    std::sort(std::begin(enemy_units), std::end(enemy_units), IsHigherPriority(this, army_centroid));

    //Find all enemy units with the highest priority
    Units HighestPriorityUnits;
    //The first entry in the enemy_units vector after sorting is one of the highest priority
    HighestPriorityUnits.push_back(enemy_units[0]); //valid since enemy_units is non-empty;
    //Find its priority level
    double HighestPriorityLevel = GetUnitAttackPriority(enemy_units[0], army_centroid);
    //Find all other units with same priority level (should be at the beginning of sorted enemy_units vector)
    for (const Unit* en_unit : enemy_units) {
        if (GetUnitAttackPriority(en_unit, army_centroid) == HighestPriorityLevel) {
            HighestPriorityUnits.push_back(en_unit);
        }
    }

    //Find all highest priority ground enemy units 
    Units HighestGroundPriorityUnits;
    double HighestGroundPriorityLevel;
    bool HighestGroundPriroityLevelFound = false;
    //Iterate through all enemy units
    for (const Unit* en_unit : enemy_units) {
        //If we havent found any ground unit while going down this sorted vector
        if (HighestGroundPriroityLevelFound == false) {
            //If we find a ground unit
            if (en_unit->is_flying == false) {
                HighestGroundPriroityLevelFound = true;
                //Set the highest priority level for ground units to be its priority level
                HighestGroundPriorityLevel = GetUnitAttackPriority(en_unit, army_centroid);
                //Add it to the HighestGroundPriorityUnits vector
                HighestGroundPriorityUnits.push_back(en_unit);
            }
        }
        //If we have found one of the highest priority ground units
        else {
            //If this unit is also ground unit and also has same priority as the highest ground priority level
            if (en_unit->is_flying == false && GetUnitAttackPriority(en_unit, army_centroid) == HighestGroundPriorityLevel) {
                //Add it to the HighestGroundPriorityUnits vector
                HighestGroundPriorityUnits.push_back(en_unit);
            }
        }
    }

    const Unit* airUnitToAttack = GetClosestUnit(army_centroid, HighestPriorityUnits);
    const Unit* groundUnitToAttack = GetClosestUnit(army_centroid, HighestGroundPriorityUnits);
    const Point2D strictlyClosestUnitPos = GetClosestUnit(army_centroid, enemy_units)->pos;

    //Iterate through all our units that are on offense
    for (const Unit* myUnit : units) {
        // If the unit can attack flying units, then attack the closest highest priority unit to the group (focus fire)
        if (CanAttackAirUnits(myUnit)) {
            Actions()->UnitCommand(myUnit, ABILITY_ID::ATTACK, airUnitToAttack);
        }
        // If the unit can't attack flying units, teh attack the closest highest priority ground unit to the group (focus fire)
        else if (!HighestGroundPriorityUnits.empty()) {
            Actions()->UnitCommand(myUnit, ABILITY_ID::ATTACK, groundUnitToAttack);
        }
        // If they don't have a good option, attack move in the right direction
        else
        {
            Actions()->UnitCommand(myUnit, ABILITY_ID::ATTACK, strictlyClosestUnitPos);
        }
    }
}

void BetaStar::StalkerBlinkMicro()
{
    if (!m_blink_researched)
        return;

    Units stalkers = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_STALKER);
    if (stalkers.empty())
    {
        return;
    }

    float weaponDist = all_unit_type_data[UnitTypeID(UNIT_TYPEID::PROTOSS_STALKER)].weapons[0].range;
    Point2D enemyCentroid = GetUnitsCentroid(Observation()->GetUnits(Unit::Alliance::Enemy));

    // THIS IS NOT ACCURATE - BLINK DOESN'T REALLY HAVE 500 RANGE
    //float blinkDist = all_ability_data[AbilityID(ABILITY_ID::EFFECT_BLINK)].cast_range;
    float blinkDist = 8.0f;
    // at what shield percentage to blink away
    float blinkBackThreshold = 0.1f;

    for (const auto& stalker : stalkers) {
        if (stalker->shield / stalker->shield_max < blinkBackThreshold) {
            Point2D blink_location = m_starting_pos;
            if (!stalker->orders.empty()) {
                Point2D diff = stalker->pos - enemyCentroid;
                Normalize2D(diff);
                blink_location = stalker->pos + diff * blinkDist;
                Actions()->UnitCommand(stalker, ABILITY_ID::EFFECT_BLINK, blink_location);
            }
        }
        else if (!stalker->orders.empty())
        {
            Point2D targetPoint(0.0f, 0.0f);
            float targetSize = 0.0f;
            const Unit *targetUnit = Observation()->GetUnit(stalker->engaged_target_tag);
            // If we couldn't get a valid engaged unit, try to get the target of our next order
            if (targetUnit == nullptr)
            {
                targetUnit = Observation()->GetUnit(stalker->orders[0].target_unit_tag);
            }
            // If we've found a valid enemy target, use its position
            if (targetUnit != nullptr)
            {
                targetPoint = targetUnit->pos;
                targetSize = targetUnit->radius;
            }
            // If we still don't have a valid enemy target, see if we have a position that our order is targeting
            //else
            //{
            //    targetPoint = stalker->orders[0].target_pos;
            //}

            // Exit if we haven't found a valid target point
            if (AlmostEqual(targetPoint, Point2D(0.0f, 0.0f)))
                return;

            float crowDist = Distance2D(targetPoint, stalker->pos);

            // Make sure the unit isn't already within weapons range of target before we blink
            if (crowDist > weaponDist + targetSize)
            {
                float modifiedBlinkDist = blinkDist;
                if (crowDist < blinkDist)
                {
                    // min blink is just in weapons range
                    float minDist = crowDist - (weaponDist + targetSize);
                    // max blink still leaves some room between stalkers and the unit
                    float maxDist = crowDist - ((weaponDist / 2.0f) + targetSize);
                    modifiedBlinkDist = minDist + abs(GetRandomScalar() * (maxDist - minDist));
                }

                // TODO: If we don't have a unit target, we're probably moving. If we're moving, we could use the unit's
                //       actual facing direction for blinks instead of wonky blinking directly toward the enemy base
                Point2D diff = targetPoint - stalker->pos;
                Normalize2D(diff);
                Point2D blink_location = stalker->pos + (diff * modifiedBlinkDist);
                Actions()->UnitCommand(stalker, ABILITY_ID::EFFECT_BLINK, blink_location);
            }
        }
    }
}

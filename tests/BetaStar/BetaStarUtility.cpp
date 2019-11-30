#include "BetaStar.h"

using namespace sc2;


// returns the nearest neutral unit of type target_unit_type
// if there is none, returns nullptr
const Unit* BetaStar::FindNearestNeutralUnit(const Point2D& start, UnitTypeID target_unit_type) {
    Units units = Observation()->GetUnits(Unit::Alliance::Neutral);

    float distance = std::numeric_limits<float>::max();
    const Unit* target = nullptr;

    for (const auto& unit : units) {
        if (unit->unit_type == target_unit_type) {
            float d = DistanceSquared2D(unit->pos, start);
            if (d < distance) {
                distance = d;
                target = unit;
            }
        }
    }

    return target;
}

// returns a vector containing friendly units of type unit_type
const Units BetaStar::FriendlyUnitsOfType(UnitTypeID unit_type) const
{
    return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
}

// returns an uncapped mineral or vespene geyser, or the closest mineral if they are all capped
const Unit* BetaStar::FindResourceToGather(Point2D unit_pos) {
    for (const auto& cc : FriendlyUnitsOfType(m_base_typeid)) {
        if (cc->assigned_harvesters < cc->ideal_harvesters) {
            return FindNearestNeutralUnit(cc->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
        }
    }
    for (const auto& vg : FriendlyUnitsOfType(m_gas_building_typeid)) {
        if (vg->assigned_harvesters < vg->ideal_harvesters) {
            return vg;
        }
    }
    return FindNearestNeutralUnit(unit_pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
}

bool BetaStar::TrainUnit(UnitTypeID unitType)
{
    const UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    // Only select unit production buildings that aren't doing anything right now
    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, BetterIsUnit(unitBuilder, HasNoOrders()));
    // warpgates = gateways, so we can build at them too
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY)
    {
        Units warpgates = Observation()->GetUnits(Unit::Alliance::Self, BetterIsUnit(UNIT_TYPEID::PROTOSS_WARPGATE, HasNoOrders()));
        if (!warpgates.empty())
        {
            buildings.insert(buildings.end(), warpgates.begin(), warpgates.end());
        }
    }

    // can't build at buildings that aren't built yet
    for (auto iter = buildings.begin(); iter != buildings.end(); )
    {
        if ((*iter)->build_progress < 1.0f)
        {
            iter = buildings.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // we don't have any building that can build this unit
    if (buildings.size() == 0)
    {
        return false;
    }

    // get random building from vector and build unit there
    return TrainUnit(GetRandomEntry(buildings), unitType);
}

bool BetaStar::TrainUnit(const Unit *building, UnitTypeID unitType)
{
    // note that all TrainUnit variants resolve themselves here, so this is where we can do all of our
    // checks and handle edge-cases

    // if a building isn't finished, it can't build units
    if (building->build_progress < 1.0f)
    {
        return false;
    }

    // only the nexus can build units without power
    if (building->unit_type != UNIT_TYPEID::PROTOSS_NEXUS && !building->is_powered)
    {
        return false;
    }

    UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    // building at a warpgate instead of a gateway (warp close to warpgate)
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY && building->unit_type == UNIT_TYPEID::PROTOSS_WARPGATE)
    {
        // if we're attacking, warp as close to the enemy as possible
        if (m_attacking)
        {
            return WarpUnit(building, m_enemy_base_pos, unitType);
        }
        // if we're building up forces, stick near production building
        else
        {
            return WarpUnit(building, building->pos, unitType);
        }
    }
    // normal training process
    else if (unitBuilder == building->unit_type)
    {
        AbilityID buildAbility = GetUnitBuildAbility(unitType);
        for (UnitOrder order : building->orders)
        {
            // if we have this unit queued to build already, don't queue another one
            if (order.ability_id == buildAbility)
            {
                return false;
            }
        }

        // Return true if training succeeds, false otherwise
        return TryIssueCommand(building, buildAbility);
    }

    // couldn't build that unit at that building - should probably never reach this exit point
    return false;
}

bool BetaStar::WarpUnit(Point2D warpLocation, UnitTypeID unitType)
{
    Units warpgates = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE);

    // we don't have any warpgates
    if (warpgates.size() == 0)
    {
        return false;
    }

    // get random warpgate and warp unit in with that warpgate
    return WarpUnit(GetRandomEntry(warpgates), warpLocation, unitType);
}

bool BetaStar::WarpUnit(const Unit *building, Point2D warpLocation, UnitTypeID unitType)
{
    // if a building isn't finished, it can't build units
    if (building->build_progress < 1.0f)
    {
        return false;
    }

    // if our warpgate isn't powered, it can't build units
    if (!building->is_powered)
    {
        return false;
    }

    const ObservationInterface* observation = Observation();
    const GameInfo gameInfo = observation->GetGameInfo();

    std::vector<PowerSource> powerSources = observation->GetPowerSources();

    if (powerSources.empty())
    {
        return false;
    }

    // find the closest power source to the specified point to warp in around
    size_t minIndex = 0;
    float minDist = DistanceSquared2D(warpLocation, powerSources[0].position);
    for (size_t i = 1; i < powerSources.size(); ++i)
    {
        float testDist = DistanceSquared2D(warpLocation, powerSources[i].position);
        if (testDist < minDist)
        {
            minDist = testDist;
            minIndex = i;
        }
    }

    const PowerSource &targetPowerSource = powerSources[minIndex];
    float randomX = GetRandomScalar();
    float randomY = GetRandomScalar();
    Point2D finalWarpPoint = Point2D(targetPowerSource.position.x + randomX * targetPowerSource.radius, targetPowerSource.position.y + randomY * targetPowerSource.radius);

    // If the warp location is walled off, don't warp there.
    // We check this to see if there is pathing from the build location to the center of the map
    if (Query()->PathingDistance(finalWarpPoint, Point2D(gameInfo.playable_max.x / 2, gameInfo.playable_max.y / 2)) < .01f) {
        return false;
    }

    // If this succeeds, return true. Otherwise, return false.
    return TryIssueCommand(building, GetUnitWarpAbility(unitType), finalWarpPoint);
}

size_t BetaStar::MassTrainUnit(UnitTypeID unitType)
{
    UnitTypeID unitBuilder = GetUnitBuilder(unitType);

    Units buildings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unitBuilder));
    // warpgates = gateways, so build for them too
    if (unitBuilder == UNIT_TYPEID::PROTOSS_GATEWAY)
    {
        Units warpgates = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::PROTOSS_WARPGATE));
        if (!warpgates.empty())
        {
            buildings.insert(buildings.end(), warpgates.begin(), warpgates.end());
        }
    }

    size_t buildCount = 0;
    for (const Unit* building : buildings)
    {
        if (TrainUnit(building, unitType))
        {
            ++buildCount;
        }
    }

    return buildCount;
}

void BetaStar::TryBuildStructureNearPylon(AbilityID ability_type_for_structure, UnitTypeID unit_type) {
    const ObservationInterface* observation = Observation();

    std::vector<PowerSource> power_sources = observation->GetPowerSources();

    if (power_sources.empty()) {
        return;
    }

    int num_tries = 10;

    for (int i = 0; i < num_tries; ++i) {
        const PowerSource& random_power_source = GetRandomEntry(power_sources);
        
        if (observation->GetUnit(random_power_source.tag)->unit_type == UNIT_TYPEID::PROTOSS_WARPPRISM) {
            continue;
        }

        float radius = random_power_source.radius;
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        Point2D build_location = Point2D(random_power_source.position.x + rx * radius, random_power_source.position.y + ry * radius);

        if (Query()->Placement(ability_type_for_structure, build_location)) {
            Units workers = FriendlyUnitsOfType(unit_type);
            for (const auto& worker : workers) {
                for (const auto& order : worker->orders) {
                    if (order.ability_id == ability_type_for_structure) {
                        return;
                    }
                }
            }
            const Unit* closest_worker = nullptr;
            float closest_distance = std::numeric_limits<float>::max();
            for (const auto& worker : workers) {
                float distance = DistanceSquared2D(build_location, worker->pos);
                if (distance < closest_distance) {
                    closest_worker = worker;
                    closest_distance = distance;
                }
            }
            if (closest_worker == nullptr) {
                return;
            }

            // try to build the structure in the valid build location
            if (TryIssueCommand(closest_worker, ability_type_for_structure, build_location)) {
                // Build valid, add info
                //bool seen = false;
                //for (const auto& building : m_buildings) {
                //    if (build_location == position) {
                //        seen = true; // Don't add to vector
                //    }
                //}
                //if (!seen) {
                m_buildings.push_back(std::make_tuple(build_location, ability_type_for_structure));
                //}
            }

            // if we get here, we either succeeded or can't succeed. Exit.
            return;
        }
    }
}

bool BetaStar::TryBuildStructureNearPylon(UnitTypeID buildingType, const Unit *builder)
{
    const ObservationInterface *observation = Observation();

    AbilityID buildAbility = GetUnitBuildAbility(buildingType);

    Units allBuilders = FriendlyUnitsOfType(m_worker_typeid);

    // We don't have any workers. GG.
    if (allBuilders.empty())
    {
        return false;
    }

    // Don't try to build another building until the current one is finished
    for (const Unit *unit : allBuilders)
    {
        for (UnitOrder order : unit->orders)
        {
            if (order.ability_id == buildAbility)
            {
                return false;
            }
        }
    }

    // Get all power sources (radius property differs from Unit radius property for pylons)
    std::vector<PowerSource> powerSources = observation->GetPowerSources();

    // Clean out Warp Prisms since we only want to build by pylons
    for (auto iter = powerSources.begin(); iter != powerSources.end(); )
    {
        UnitTypeID testType = observation->GetUnit(iter->tag)->unit_type;
        if (testType == UNIT_TYPEID::PROTOSS_WARPPRISM || testType == UNIT_TYPEID::PROTOSS_WARPPRISMPHASING)
        {
            iter = powerSources.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // no pylons on the map. GG.
    if (powerSources.empty())
    {
        return false;
    }

    const PowerSource &chosenPylon = GetRandomEntry(powerSources);

    Point2D buildPos = Point2D(chosenPylon.position.x + GetRandomScalar()*chosenPylon.radius, chosenPylon.position.y + GetRandomScalar()*chosenPylon.radius);

    // if we don't have a builder selected, get the closest worker
    if (builder == nullptr)
    {
        GetClosestUnit(buildPos, allBuilders);
    }

    // We have everything we need to attempt construction
    return TryBuildStructure(buildingType, buildPos, builder);
}

bool BetaStar::TryBuildStructureNearPylon(UnitTypeID buildingType, Point2D nearPosition, const Unit *builder)
{
    const ObservationInterface *observation = Observation();

    AbilityID buildAbility = GetUnitBuildAbility(buildingType);

    Units allBuilders = FriendlyUnitsOfType(m_worker_typeid);

    // We don't have any workers. GG.
    if (allBuilders.empty())
    {
        return false;
    }

    // Don't try to build another building until the current one is finished
    for (const Unit *unit : allBuilders)
    {
        for (UnitOrder order : unit->orders)
        {
            if (order.ability_id == buildAbility)
            {
                return false;
            }
        }
    }

    // Get all power sources (radius property differs from Unit radius property for pylons)
    std::vector<PowerSource> powerSources = observation->GetPowerSources();

    // Clean out Warp Prisms since we only want to build by pylons
    for (auto iter = powerSources.begin(); iter != powerSources.end(); )
    {
        UnitTypeID testType = observation->GetUnit(iter->tag)->unit_type;
        if (testType == UNIT_TYPEID::PROTOSS_WARPPRISM || testType == UNIT_TYPEID::PROTOSS_WARPPRISMPHASING)
        {
            iter = powerSources.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // no pylons on the map. GG.
    if (powerSources.empty())
    {
        return false;
    }

    size_t minIndex = 0;
    float minDistSqrd = DistanceSquared2D(powerSources[minIndex].position, nearPosition);
    for (size_t index = 1; index < powerSources.size(); ++index)
    {
        float testDistSqrd = DistanceSquared2D(powerSources[index].position, nearPosition);
        if (testDistSqrd < minDistSqrd)
        {
            minIndex = index;
            minDistSqrd = testDistSqrd;
        }
    }

    const PowerSource &chosenPylon = powerSources[minIndex];

    Point2D buildPos = Point2D(chosenPylon.position.x + GetRandomScalar()*chosenPylon.radius, chosenPylon.position.y + GetRandomScalar()*chosenPylon.radius);

    // if we don't have a builder selected, get the closest worker
    if (builder == nullptr)
    {
        GetClosestUnit(buildPos, allBuilders);
    }

    // We have everything we need to attempt construction
    return TryBuildStructure(buildingType, buildPos, builder);
}

bool BetaStar::TryBuildStructureNearPylon(UnitTypeID buildingType, Point2D nearPosition, float maxRadius, const Unit *builder)
{
    const ObservationInterface *observation = Observation();

    AbilityID buildAbility = GetUnitBuildAbility(buildingType);

    Units allBuilders = FriendlyUnitsOfType(m_worker_typeid);

    // We don't have any workers. GG.
    if (allBuilders.empty())
    {
        return false;
    }

    // Don't try to build another building until the current one is finished
    for (const Unit *unit : allBuilders)
    {
        for (UnitOrder order : unit->orders)
        {
            if (order.ability_id == buildAbility)
            {
                return false;
            }
        }
    }

    // Get all power sources (radius property differs from Unit radius property for pylons)
    std::vector<PowerSource> powerSources = observation->GetPowerSources();

    float radiusSqrd = maxRadius * maxRadius;
    // Clean out Warp Prisms since we only want to build by pylons
    // Also clean out pylons not within the specified radius
    for (auto iter = powerSources.begin(); iter != powerSources.end(); )
    {
        UnitTypeID testType = observation->GetUnit(iter->tag)->unit_type;
        if (testType == UNIT_TYPEID::PROTOSS_WARPPRISM || testType == UNIT_TYPEID::PROTOSS_WARPPRISMPHASING)
        {
            iter = powerSources.erase(iter);
        }
        else if (DistanceSquared2D(iter->position, nearPosition) > radiusSqrd)
        {
            iter = powerSources.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // no valid pylons
    if (powerSources.empty())
    {
        return false;
    }

    const PowerSource &chosenPylon = GetRandomEntry(powerSources);

    Point2D buildPos = Point2D(chosenPylon.position.x + GetRandomScalar()*chosenPylon.radius, chosenPylon.position.y + GetRandomScalar()*chosenPylon.radius);

    // if we don't have a builder selected, get the closest worker
    if (builder == nullptr)
    {
        GetClosestUnit(buildPos, allBuilders);
    }

    // We have everything we need to attempt construction
    return TryBuildStructure(buildingType, buildPos, builder);
}

bool BetaStar::TryBuildStructure(UnitTypeID buildingType, Point2D buildPos, const Unit *builder)
{
    AbilityID buildAbility = GetUnitBuildAbility(buildingType);

    // Test building placement at that location to make sure nothing is in the way
    if (Query()->Placement(buildAbility, buildPos))
    {
        // The builder can't build there if they can't get there
        if (!AlmostEqual(Query()->PathingDistance(builder, buildPos), 0.0f))
        {
            // try to build the structure in the valid build location
            if (TryIssueCommand(builder, buildAbility, buildPos))
            {
                // Pylons are managed in a different place
                if (buildingType != m_supply_building_typeid)
                {
                    m_buildings.push_back(std::make_tuple(buildPos, buildAbility));
                }
                return true;
            }
        }
    }

    // Couldn't build
    return false;
}

void BetaStar::TryResearchUpgrade(AbilityID upgrade_abilityid, UnitTypeID building_type)
{
    const ObservationInterface* observation = Observation();

    Units buildings = FriendlyUnitsOfType(building_type);

    // check whether the upgrade is already being researched
    for (const auto& building : buildings) {
        if (building->build_progress != 1) {
            continue;
        }
        for (const auto& order : building->orders) {
            if (order.ability_id == upgrade_abilityid) {
                return;
            }
        }
    }

    // look for unoccupied building, research upgrade
    for (const auto& building : buildings) {
        if (building->build_progress != 1) {
            continue;
        }
        if (building->orders.size() == 0) {
            // if we're successful in researching the upgrade, exit
            if (TryIssueCommand(building, upgrade_abilityid))
            {
                return;
            }
        }
    }
}

void BetaStar::ClearArmyRatios()
{
    army_ratios.clear();
}

size_t BetaStar::CountUnitType(UnitTypeID unitType, bool includeIncomplete)
{
    const ObservationInterface *observation = Observation();

    const Units allUnits = observation->GetUnits(Unit::Alliance::Self, IsUnit(unitType));

    size_t count = 0;

    // count structures differently than units
    if (IsStructure(unitType))
    {
        // buildings under construction will be included in allUnits
        if (includeIncomplete)
        {
            count = allUnits.size();
        }
        // only count buildings that are fully constructed
        else
        {
            for (const Unit *building : allUnits)
            {
                if (building->build_progress == 1.0f)
                {
                    ++count;
                }
            }
        }
    }
    else
    {
        count = allUnits.size();
        // units in the queue are not included in allUnits
        if (includeIncomplete)
        {
            const Units trainingUnits = observation->GetUnits(Unit::Alliance::Self, IsUnit(GetUnitBuilder(unitType)));
            for (const Unit *trainer : trainingUnits)
            {
                AbilityID buildAbility = GetUnitBuildAbility(unitType);
                for (UnitOrder order : trainer->orders)
                {
                    if (order.ability_id == buildAbility)
                    {
                        ++count;
                    }
                }
            }
        }
    }

    return count;
}

void BetaStar::SetStrategy(Strategy newStrategy)
{
    m_current_strategy = newStrategy;
    // Set all ratios to 0. No new units will be built automatically.
    ClearArmyRatios();

    // Set new army ratios based on the strategy (can be fine-tuned elsewhere)
    switch (newStrategy)
    {
        case Strategy::Blink_Stalker_Rush:
            army_ratios[UNIT_TYPEID::PROTOSS_STALKER] = 1.0f;
            break;
        default:
            army_ratios[UNIT_TYPEID::PROTOSS_STALKER] = 1.0f;
            break;
    }
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability);
            return true;
        }
    }
    return false;
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability, const Unit *target)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability, target);
            return true;
        }
    }
    return false;
}

bool BetaStar::TryIssueCommand(const Unit *unit, AbilityID ability, Point2D point)
{
    // Generic catch for when we can't use this ability (i.e. can't afford it, don't have the upgrades, on cooldown, etc.)
    AvailableAbilities abilities = Query()->GetAbilitiesForUnit(unit);
    for (const auto &tempAbility : abilities.abilities)
    {
        if (tempAbility.ability_id == ability)
        {
            Actions()->UnitCommand(unit, ability, point);
            return true;
        }
    }
    return false;
}

const Unit* BetaStar::GetClosestUnit(Point2D position, const Units units)
{
    // empty collections of units should never be passed
    if (units.empty())
    {
        return nullptr;
    }

    size_t minIndex = 0;
    float minDist = DistanceSquared2D(position, units[minIndex]->pos);

    for (size_t i = 1; i < units.size(); ++i)
    {
        float testDist = DistanceSquared2D(position, units[i]->pos);
        if (testDist < minDist)
        {
            minDist = testDist;
            minIndex = i;
        }
    }

    return units[minIndex];
}

Point2D BetaStar::GetUnitsCentroid(const Units units)
{
    Point2D centroid(0, 0);
    for (const Unit *unit : units)
    {
        centroid += unit->pos;
    }

    return centroid / (float)units.size();
}

Point2D BetaStar::GetUnitsCentroidNearPoint(Units units, float unitFrac, Point2D desiredTarget)
{
    // Can't work with 0 units
    if (AlmostEqual(unitFrac, 0.0f))
        return Point2D(96, 96);

    std::sort(units.begin(), units.end(), IsCloser(desiredTarget));

    int nUnits = ceil(units.size() * unitFrac);

    Point2D centroid(0.0f, 0.0f);
    for (size_t i = 0; i < nUnits; ++i)
    {
        centroid += units[i]->pos;
    }

    return centroid / (float)nUnits;
}

float BetaStar::GetGameTime()
{
    return Observation()->GetGameLoop() / 22.4f;
}

int BetaStar::GetQuadrantByPoint(Point2D pos) {
    if (pos.x < 96) {
        if (pos.y < 96) {
            return SW;
        }
        else {
            return NW;
        }
    }
    else {
        if (pos.y < 96) {
            return SE;
        }
        else {
            return NE;
        }
    }
}

// assumes points are in the south-west quadrant (x < 96, y < 96)
Point2D BetaStar::RotatePosition(Point2D pos, int new_quadrant) {
    Point2D new_pos(pos);
    new_pos.x = new_pos.x - 96;
    new_pos.y = new_pos.y - 96;
    switch (new_quadrant) {
        case (SW): {
            break;
        }
        case (NW): {
            for (int i = 0; i < 1; i++) {
                float x = new_pos.x;
                float y = new_pos.y;
                new_pos.x = y;
                new_pos.y = x * -1;
            }
            break;
        }
        case (NE): {
            for (int i = 0; i < 2; i++) {
                float x = new_pos.x;
                float y = new_pos.y;
                new_pos.x = y;
                new_pos.y = x * -1;
            }
            break;
        }
        case (SE): {
            for (int i = 0; i < 3; i++) {
                float x = new_pos.x;
                float y = new_pos.y;
                new_pos.x = y;
                new_pos.y = x * -1;
            }
            break;
        }
    }
    new_pos.x = new_pos.x + 96;
    new_pos.y = new_pos.y + 96;
    return new_pos;
}

UnitTypeID BetaStar::GetUnitBuilder(UnitTypeID unitToBuild)
{
    // TODO: Warp gates are not represented as a builder
    switch (unitToBuild.ToType())
    {
        // Protoss Units
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_CARRIER:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_COLOSSUS:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_DISRUPTOR:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_FORGE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_GATEWAY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_IMMORTAL:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_INTERCEPTOR:
            return UNIT_TYPEID::PROTOSS_CARRIER;
        case UNIT_TYPEID::PROTOSS_MOTHERSHIP:
            return UNIT_TYPEID::PROTOSS_NEXUS;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_ORACLE:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_PHOENIX:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return UNIT_TYPEID::PROTOSS_NEXUS;
        case UNIT_TYPEID::PROTOSS_PYLON:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_SHIELDBATTERY:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return UNIT_TYPEID::PROTOSS_GATEWAY;
        case UNIT_TYPEID::PROTOSS_STARGATE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_TEMPEST:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
            return UNIT_TYPEID::PROTOSS_PROBE;
        case UNIT_TYPEID::PROTOSS_VOIDRAY:
            return UNIT_TYPEID::PROTOSS_STARGATE;
        case UNIT_TYPEID::PROTOSS_WARPPRISM:
            return UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return UNIT_TYPEID::PROTOSS_GATEWAY;

            // Terran Units - no need to complete
        case UNIT_TYPEID::TERRAN_BARRACKS:
            return UNIT_TYPEID::TERRAN_SCV;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
            return UNIT_TYPEID::TERRAN_SCV;
        case UNIT_TYPEID::TERRAN_MARINE:
            return UNIT_TYPEID::TERRAN_BARRACKS;
        case UNIT_TYPEID::TERRAN_SCV:
            return UNIT_TYPEID::TERRAN_COMMANDCENTER;

            // Should never be reached
        default:
            // Program broke because unit you asked for wasn't added to the switch case yet. Add it.
            std::cout << "ERROR: GetUnitBuilder could not find a builder for [" << unitToBuild.to_string() << "]. Add this to the switch case." << std::endl;
            return UNIT_TYPEID::INVALID;
    }
}

AbilityID BetaStar::GetUnitBuildAbility(UnitTypeID unitToBuild)
{
    // TODO: Warping units is a different action. Not represented.
    switch (unitToBuild.ToType())
    {
        // Protoss Build Actions
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return ABILITY_ID::TRAIN_ADEPT;
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
            return ABILITY_ID::BUILD_ASSIMILATOR;
        case UNIT_TYPEID::PROTOSS_CARRIER:
            return ABILITY_ID::TRAIN_CARRIER;
        case UNIT_TYPEID::PROTOSS_COLOSSUS:
            return ABILITY_ID::TRAIN_COLOSSUS;
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
            return ABILITY_ID::BUILD_CYBERNETICSCORE;
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
            return ABILITY_ID::BUILD_DARKSHRINE;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return ABILITY_ID::TRAIN_DARKTEMPLAR;
        case UNIT_TYPEID::PROTOSS_DISRUPTOR:
            return ABILITY_ID::TRAIN_DISRUPTOR;
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
            return ABILITY_ID::BUILD_FLEETBEACON;
        case UNIT_TYPEID::PROTOSS_FORGE:
            return ABILITY_ID::BUILD_FORGE;
        case UNIT_TYPEID::PROTOSS_GATEWAY:
            return ABILITY_ID::BUILD_GATEWAY;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return ABILITY_ID::TRAIN_HIGHTEMPLAR;
        case UNIT_TYPEID::PROTOSS_IMMORTAL:
            return ABILITY_ID::TRAIN_IMMORTAL;
        case UNIT_TYPEID::PROTOSS_INTERCEPTOR:
            return ABILITY_ID::BUILD_INTERCEPTORS;
        case UNIT_TYPEID::PROTOSS_MOTHERSHIP:
            return ABILITY_ID::TRAIN_MOTHERSHIP;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return ABILITY_ID::BUILD_NEXUS;
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return ABILITY_ID::TRAIN_OBSERVER;
        case UNIT_TYPEID::PROTOSS_ORACLE:
            return ABILITY_ID::TRAIN_ORACLE;
        case UNIT_TYPEID::PROTOSS_PHOENIX:
            return ABILITY_ID::TRAIN_PHOENIX;
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
            return ABILITY_ID::BUILD_PHOTONCANNON;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return ABILITY_ID::TRAIN_PROBE;
        case UNIT_TYPEID::PROTOSS_PYLON:
            return ABILITY_ID::BUILD_PYLON;
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
            return ABILITY_ID::BUILD_ROBOTICSBAY;
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
            return ABILITY_ID::BUILD_ROBOTICSFACILITY;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return ABILITY_ID::TRAIN_SENTRY;
        case UNIT_TYPEID::PROTOSS_SHIELDBATTERY:
            return ABILITY_ID::BUILD_SHIELDBATTERY;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return ABILITY_ID::TRAIN_STALKER;
        case UNIT_TYPEID::PROTOSS_STARGATE:
            return ABILITY_ID::BUILD_STARGATE;
        case UNIT_TYPEID::PROTOSS_TEMPEST:
            return ABILITY_ID::TRAIN_TEMPEST;
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
            return ABILITY_ID::BUILD_TEMPLARARCHIVE;
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
            return ABILITY_ID::BUILD_TWILIGHTCOUNCIL;
        case UNIT_TYPEID::PROTOSS_VOIDRAY:
            return ABILITY_ID::TRAIN_VOIDRAY;
        case UNIT_TYPEID::PROTOSS_WARPPRISM:
            return ABILITY_ID::TRAIN_WARPPRISM;
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return ABILITY_ID::TRAIN_ZEALOT;

            // Terran Build Actions - no need to complete
        case UNIT_TYPEID::TERRAN_BARRACKS:
            return ABILITY_ID::BUILD_BARRACKS;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
            return ABILITY_ID::BUILD_COMMANDCENTER;
        case UNIT_TYPEID::TERRAN_MARINE:
            return ABILITY_ID::TRAIN_MARINE;
        case UNIT_TYPEID::TERRAN_SCV:
            return ABILITY_ID::TRAIN_SCV;

            // Should never be reached
        default:
            // Program broke because unit you asked for wasn't added to the switch case yet. Add it.
            std::cout << "ERROR: GetUnitBuildAbility could not find a build command for [" << unitToBuild.to_string() << "]. Add this to the switch case." << std::endl;
            return ABILITY_ID::INVALID;
    }
}

AbilityID BetaStar::GetUnitWarpAbility(UnitTypeID unitToWarp)
{
    switch (unitToWarp.ToType())
    {
        case UNIT_TYPEID::PROTOSS_ZEALOT:
            return ABILITY_ID::TRAINWARP_ZEALOT;
        case UNIT_TYPEID::PROTOSS_SENTRY:
            return ABILITY_ID::TRAINWARP_SENTRY;
        case UNIT_TYPEID::PROTOSS_STALKER:
            return ABILITY_ID::TRAINWARP_STALKER;
        case UNIT_TYPEID::PROTOSS_ADEPT:
            return ABILITY_ID::TRAINWARP_ADEPT;
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
            return ABILITY_ID::TRAINWARP_HIGHTEMPLAR;
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
            return ABILITY_ID::TRAINWARP_DARKTEMPLAR;
            // unit cannot be warped
        default:
            return ABILITY_ID::INVALID;
    }
}

bool BetaStar::IsStructure(UnitTypeID unitType)
{
    switch (unitType.ToType())
    {
        case UNIT_TYPEID::PROTOSS_ASSIMILATOR:
        case UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
        case UNIT_TYPEID::PROTOSS_DARKSHRINE:
        case UNIT_TYPEID::PROTOSS_FLEETBEACON:
        case UNIT_TYPEID::PROTOSS_FORGE:
        case UNIT_TYPEID::PROTOSS_GATEWAY:
        case UNIT_TYPEID::PROTOSS_NEXUS:
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
        case UNIT_TYPEID::PROTOSS_PYLON:
        case UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED:
        case UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
        case UNIT_TYPEID::PROTOSS_STARGATE:
        case UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
        case UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
        case UNIT_TYPEID::PROTOSS_WARPGATE:
        case UNIT_TYPEID::TERRAN_ARMORY:
        case UNIT_TYPEID::TERRAN_AUTOTURRET:
        case UNIT_TYPEID::TERRAN_BARRACKS:
        case UNIT_TYPEID::TERRAN_BARRACKSFLYING:
        case UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
        case UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
        case UNIT_TYPEID::TERRAN_BUNKER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
        case UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
        case UNIT_TYPEID::TERRAN_FACTORY:
        case UNIT_TYPEID::TERRAN_FACTORYFLYING:
        case UNIT_TYPEID::TERRAN_FACTORYREACTOR:
        case UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
        case UNIT_TYPEID::TERRAN_FUSIONCORE:
        case UNIT_TYPEID::TERRAN_GHOSTACADEMY:
        case UNIT_TYPEID::TERRAN_MISSILETURRET:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
        case UNIT_TYPEID::TERRAN_POINTDEFENSEDRONE:
        case UNIT_TYPEID::TERRAN_REACTOR:
        case UNIT_TYPEID::TERRAN_REFINERY:
        case UNIT_TYPEID::TERRAN_SENSORTOWER:
        case UNIT_TYPEID::TERRAN_STARPORT:
        case UNIT_TYPEID::TERRAN_STARPORTFLYING:
        case UNIT_TYPEID::TERRAN_STARPORTREACTOR:
        case UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
        case UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
        case UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
        case UNIT_TYPEID::TERRAN_TECHLAB:
        case UNIT_TYPEID::ZERG_BANELINGNEST:
        case UNIT_TYPEID::ZERG_CREEPTUMOR:
        case UNIT_TYPEID::ZERG_CREEPTUMORBURROWED:
        case UNIT_TYPEID::ZERG_CREEPTUMORQUEEN:
        case UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
        case UNIT_TYPEID::ZERG_EXTRACTOR:
        case UNIT_TYPEID::ZERG_GREATERSPIRE:
        case UNIT_TYPEID::ZERG_HATCHERY:
        case UNIT_TYPEID::ZERG_HIVE:
        case UNIT_TYPEID::ZERG_HYDRALISKDEN:
        case UNIT_TYPEID::ZERG_INFESTATIONPIT:
        case UNIT_TYPEID::ZERG_LAIR:
        case UNIT_TYPEID::ZERG_NYDUSCANAL:
        case UNIT_TYPEID::ZERG_NYDUSNETWORK:
        case UNIT_TYPEID::ZERG_ROACHWARREN:
        case UNIT_TYPEID::ZERG_SPAWNINGPOOL:
        case UNIT_TYPEID::ZERG_SPIRE:
        case UNIT_TYPEID::ZERG_SPINECRAWLER:
        case UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED:
        case UNIT_TYPEID::ZERG_SPORECRAWLER:
        case UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED:
        case UNIT_TYPEID::ZERG_ULTRALISKCAVERN:
            return true;
        default:
            return false;
    }
}

bool BetaStar::AlmostEqual(float lhs, float rhs, float threshold)
{
    return abs(lhs - rhs) <= threshold;
}

bool BetaStar::AlmostEqual(Point2D lhs, Point2D rhs, Point2D threshold)
{
    Point2D diff = lhs - rhs;
    return abs(diff.x) <= threshold.x && abs(diff.y) <= threshold.y;
}

//Returns an integer:
// Between 0 and 100 for structural units
// Between 100 and 200 for low priority units
// Between 200 and 300 for medium priority units
// Between 300 and 400 for high priority units
// The return value has been made to be a range so that we can vary priority values within the range if we like

int BetaStar::GetUnitAttackPriority(const Unit* unit, Point2D army_centroid) {
    double distance_to_army = DistanceSquared2D(unit->pos, army_centroid);
    double distance_weight = 0.1;
    switch (enemy_race) {
    case Race::Protoss:
        return GetProtossUnitAttackPriority(unit) - (distance_to_army * distance_weight);
    case Race::Terran:
        return GetTerranUnitAttackPriority(unit) - (distance_to_army * distance_weight);
    case Race::Zerg:
        return GetZergUnitAttackPriority(unit) - (distance_to_army * distance_weight);
    case Race::Random:
        std::cout << "Enemy Race not yet detected";
        return -1;
    default:
        std::cout << "Should not be reached";
        return -2;
    }
}

int BetaStar::GetProtossUnitAttackPriority(const Unit* unit)  {
    // Order in descending priority, except for the default case
    switch ((unit->unit_type).ToType()) {
        case UNIT_TYPEID::PROTOSS_DARKTEMPLAR:
        case UNIT_TYPEID::PROTOSS_WARPPRISM:
        case UNIT_TYPEID::PROTOSS_MOTHERSHIP:
            return 400;
        case UNIT_TYPEID::PROTOSS_DISRUPTOR:
        case UNIT_TYPEID::PROTOSS_HIGHTEMPLAR:
        case UNIT_TYPEID::PROTOSS_SENTRY:
        case UNIT_TYPEID::PROTOSS_PHOTONCANNON:
            return 300;
        case UNIT_TYPEID::PROTOSS_IMMORTAL:
            return 225;
        // default units with weapons will be 199
        case UNIT_TYPEID::PROTOSS_OBSERVER:
            return AlmostEqual(army_ratios[UNIT_TYPEID::PROTOSS_DARKTEMPLAR], 0.0f) ? 0 : 150;
        case UNIT_TYPEID::PROTOSS_PYLON:
            return 125;
        case UNIT_TYPEID::PROTOSS_NEXUS:
            return 115;
        case UNIT_TYPEID::PROTOSS_PROBE:
            return 110;
        case UNIT_TYPEID::PROTOSS_GATEWAY:
        case UNIT_TYPEID::PROTOSS_WARPGATE:
        case UNIT_TYPEID::PROTOSS_STARGATE:
        case UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
            return 100;
        // default units without weapons will be 99

        // default buildings without weapons will be 1
        default:
            return GenericPriorityFallbacks(unit);
    }
}

int BetaStar::GetTerranUnitAttackPriority(const Unit* unit) {
    // Order in descending priority, except for the default case
    switch ((unit->unit_type).ToType()) {
        case UNIT_TYPEID::TERRAN_GHOST:
        case UNIT_TYPEID::TERRAN_SIEGETANK:
            return 400;
        case UNIT_TYPEID::TERRAN_BATTLECRUISER:
            return 300;
        case UNIT_TYPEID::TERRAN_MEDIVAC:
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
            return 200;
        // default units with weapons will be 199
        case UNIT_TYPEID::TERRAN_MARINE:
        case UNIT_TYPEID::TERRAN_REAPER:
            return 150;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER:
        case UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
            return 125;
        case UNIT_TYPEID::TERRAN_SCV:
            return 110;
        case UNIT_TYPEID::TERRAN_FACTORY:
        case UNIT_TYPEID::TERRAN_BARRACKS:
        case UNIT_TYPEID::TERRAN_STARPORT:
            return 100;
        // default units without weapons will be 99
        // default buildings without weapons will be 1
        default:
            return GenericPriorityFallbacks(unit);
    }
}

int BetaStar::GetZergUnitAttackPriority(const Unit* unit) {
    // Order in descending priority, except for the default case
    switch ((unit->unit_type).ToType()) {
        case UNIT_TYPEID::ZERG_BROODLORD:
        case UNIT_TYPEID::ZERG_ULTRALISK:
            return 400;
        case UNIT_TYPEID::ZERG_VIPER:
        case UNIT_TYPEID::ZERG_SWARMHOSTMP:
        case UNIT_TYPEID::ZERG_INFESTOR:
        case UNIT_TYPEID::ZERG_LURKERMP:
            return 300;
        // default units with weapons will be 199
        case UNIT_TYPEID::ZERG_HATCHERY:
        case UNIT_TYPEID::ZERG_LAIR:
        case UNIT_TYPEID::ZERG_HIVE:
            return 100;
        // default units without weapons will be 99
        case UNIT_TYPEID::ZERG_DRONE:
            return 75;
        case UNIT_TYPEID::ZERG_OVERSEER:
        case UNIT_TYPEID::ZERG_OVERLORD:
            return 50;
        // default buildings without weapons will be 1
        case UNIT_TYPEID::ZERG_LARVA:
        case UNIT_TYPEID::ZERG_BROODLORDCOCOON:
        case UNIT_TYPEID::ZERG_RAVAGERCOCOON:
        case UNIT_TYPEID::ZERG_OVERLORDCOCOON:
        case UNIT_TYPEID::ZERG_TRANSPORTOVERLORDCOCOON:
        case UNIT_TYPEID::ZERG_EGG:
            return 0;
        default:
            return GenericPriorityFallbacks(unit);
    }
}

int BetaStar::GenericPriorityFallbacks(const Unit* unit)
{
    // Units with weapons are more dangerous
    if (all_unit_type_data[unit->unit_type].weapons.size() > 0)
    {
        return 199;
    }
    // Of the units that can't attack, structures are lowest priority (leave room for 0 priority units)
    else
    {
        return IsStructure(unit->unit_type) ? 1 : 99;
    }
}

bool BetaStar::CanAttackAirUnits(const Unit* unit) {
    for (Weapon w : all_unit_type_data[unit->unit_type].weapons) {
        if (w.type == Weapon::TargetType::Air || w.type == Weapon::TargetType::Any) {
            return true;
        }
    }
    return false;
}
